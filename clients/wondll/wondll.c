/*
 * WONDLL.dll — drop-in replacement for the original WON.net client DLL.
 *
 * Silencer Beta 0110 loads WONDLL.dll at runtime for all auth and profile
 * operations. Placing this DLL alongside Silencer.exe intercepts those calls
 * and redirects them to our lobby server, requiring zero exe patching.
 *
 * The lobby WON-compat HTTP endpoint is configured via the SILENCER_LOBBY_URL
 * environment variable (default: http://127.0.0.1:15173).
 *
 * Build: see CMakeLists.txt — requires MinGW i686 cross-compiler.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------
 * Session state — persisted across WON function calls within one session.
 * The original WON SDK used opaque "handles"; we use a single global because
 * Silencer only ever has one logged-in user.
 * ---------------------------------------------------------------------- */
static char g_lobby_url[256] = "http://127.0.0.1:15173";
static char g_username[64]   = {0};
static char g_token[512]     = {0};
static BOOL g_logged_in      = FALSE;

/* Simple opaque handle values the game passes back into profile functions. */
#define WON_AUTH_HANDLE    ((HANDLE)0x574F4E41)   /* "WONA" */
#define WON_PROFILE_HANDLE ((HANDLE)0x574F4E50)   /* "WONP" */

/* WON status codes (subset used by Silencer). */
#define WON_STATUS_SUCCESS     0
#define WON_STATUS_FAILURE    -1
#define WON_STATUS_BADPASSWORD 6
#define WON_STATUS_BADUSER     7

/* -------------------------------------------------------------------------
 * HTTP helpers using WinInet (available on all Win9x/NT targets).
 * ---------------------------------------------------------------------- */

/* Synchronous POST — sends JSON body, writes response into out[outlen].
 * Returns HTTP status code, or 0 on network failure. */
static int http_post(const char *path, const char *json_body,
                     char *out, DWORD outlen)
{
    char host[128];
    char object[256];
    INTERNET_PORT port = INTERNET_DEFAULT_HTTP_PORT;

    /* Parse g_lobby_url → host + object */
    const char *start = g_lobby_url;
    if (strncmp(start, "http://", 7) == 0)  start += 7;
    const char *slash = strchr(start, '/');
    const char *colon = strchr(start, ':');

    if (colon && (!slash || colon < slash)) {
        int hlen = (int)(colon - start);
        strncpy(host, start, hlen); host[hlen] = '\0';
        port = (INTERNET_PORT)atoi(colon + 1);
    } else if (slash) {
        int hlen = (int)(slash - start);
        strncpy(host, start, hlen); host[hlen] = '\0';
    } else {
        strncpy(host, start, sizeof(host) - 1);
    }

    snprintf(object, sizeof(object), "%s%s",
             (slash ? slash : ""), path);

    HINTERNET hInet = InternetOpenA("WONDLL/1.0", INTERNET_OPEN_TYPE_DIRECT,
                                    NULL, NULL, 0);
    if (!hInet) return 0;

    HINTERNET hConn = InternetConnectA(hInet, host, port,
                                       NULL, NULL, INTERNET_SERVICE_HTTP,
                                       0, 0);
    if (!hConn) { InternetCloseHandle(hInet); return 0; }

    HINTERNET hReq = HttpOpenRequestA(hConn, "POST", object,
                                      NULL, NULL, NULL,
                                      INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hReq) {
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return 0;
    }

    const char *headers = "Content-Type: application/json\r\n";
    DWORD bodyLen = json_body ? (DWORD)strlen(json_body) : 0;
    BOOL ok = HttpSendRequestA(hReq, headers, (DWORD)strlen(headers),
                               (LPVOID)json_body, bodyLen);

    int status = 0;
    if (ok) {
        DWORD statusBuf = 0, statusSize = sizeof(statusBuf);
        HttpQueryInfoA(hReq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                       &statusBuf, &statusSize, NULL);
        status = (int)statusBuf;

        if (out && outlen > 0) {
            DWORD read = 0;
            InternetReadFile(hReq, out, outlen - 1, &read);
            out[read] = '\0';
        }
    }

    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hInet);
    return status;
}

/* Synchronous GET. */
static int http_get(const char *path, char *out, DWORD outlen)
{
    char host[128];
    char object[256];
    INTERNET_PORT port = INTERNET_DEFAULT_HTTP_PORT;

    const char *start = g_lobby_url;
    if (strncmp(start, "http://", 7) == 0) start += 7;
    const char *slash = strchr(start, '/');
    const char *colon = strchr(start, ':');

    if (colon && (!slash || colon < slash)) {
        int hlen = (int)(colon - start);
        strncpy(host, start, hlen); host[hlen] = '\0';
        port = (INTERNET_PORT)atoi(colon + 1);
    } else if (slash) {
        int hlen = (int)(slash - start);
        strncpy(host, start, hlen); host[hlen] = '\0';
    } else {
        strncpy(host, start, sizeof(host) - 1);
    }

    snprintf(object, sizeof(object), "%s%s",
             (slash ? slash : ""), path);

    HINTERNET hInet = InternetOpenA("WONDLL/1.0", INTERNET_OPEN_TYPE_DIRECT,
                                    NULL, NULL, 0);
    if (!hInet) return 0;

    HINTERNET hConn = InternetConnectA(hInet, host, port,
                                       NULL, NULL, INTERNET_SERVICE_HTTP,
                                       0, 0);
    if (!hConn) { InternetCloseHandle(hInet); return 0; }

    HINTERNET hReq = HttpOpenRequestA(hConn, "GET", object,
                                      NULL, NULL, NULL,
                                      INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hReq) {
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return 0;
    }

    BOOL ok = HttpSendRequestA(hReq, NULL, 0, NULL, 0);
    int status = 0;
    if (ok) {
        DWORD statusBuf = 0, statusSize = sizeof(statusBuf);
        HttpQueryInfoA(hReq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                       &statusBuf, &statusSize, NULL);
        status = (int)statusBuf;

        if (out && outlen > 0) {
            DWORD read = 0;
            InternetReadFile(hReq, out, outlen - 1, &read);
            out[read] = '\0';
        }
    }

    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hInet);
    return status;
}

/* Minimal JSON field extractor — avoids pulling in a JSON library.
 * Finds the first occurrence of "key":"value" or "key":number. */
static BOOL json_get_str(const char *json, const char *key,
                         char *out, int outlen)
{
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    const char *p = strstr(json, needle);
    if (!p) return FALSE;
    p += strlen(needle);
    while (*p == ' ') p++;
    if (*p == '"') {
        p++;
        int i = 0;
        while (*p && *p != '"' && i < outlen - 1)
            out[i++] = *p++;
        out[i] = '\0';
        return TRUE;
    }
    /* number */
    int i = 0;
    while (*p && (*p >= '0' && *p <= '9') && i < outlen - 1)
        out[i++] = *p++;
    out[i] = '\0';
    return i > 0;
}

static BOOL json_get_bool(const char *json, const char *key)
{
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    const char *p = strstr(json, needle);
    if (!p) return FALSE;
    p += strlen(needle);
    while (*p == ' ') p++;
    return strncmp(p, "true", 4) == 0;
}

/* -------------------------------------------------------------------------
 * DLL entry point.
 * ---------------------------------------------------------------------- */
BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    (void)hInst; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        /* Allow override via environment variable. */
        const char *env = getenv("SILENCER_LOBBY_URL");
        if (env && strlen(env) < sizeof(g_lobby_url))
            strncpy(g_lobby_url, env, sizeof(g_lobby_url) - 1);
    }
    return TRUE;
}

/* =========================================================================
 * AUTH FUNCTIONS
 * ======================================================================= */

/*
 * WONAuthLoginA — authenticate an existing account.
 *
 * Original signature (reconstructed from WON SDK headers):
 *   WONError WONAuthLoginA(WONIPAddress *servers, unsigned nServers,
 *                          const char *name, const char *password,
 *                          BOOL newAcct, long timeout,
 *                          HANDLE *authH, HANDLE *peerKeyH);
 *
 * We ignore the server list and timeout; name/password go to our lobby.
 */
__declspec(dllexport) int WINAPI WONAuthLoginA(
    void   *servers,    /* WONIPAddress array — ignored */
    DWORD   nServers,   /* count — ignored */
    const char *name,
    const char *password,
    BOOL    newAcct,    /* TRUE if this is also account creation */
    long    timeout,    /* ms — ignored */
    HANDLE *authH,
    HANDLE *peerKeyH)
{
    (void)servers; (void)nServers; (void)newAcct;
    (void)timeout; (void)peerKeyH;

    if (!name || !password) return WON_STATUS_FAILURE;

    char body[256], resp[512];
    snprintf(body, sizeof(body),
             "{\"name\":\"%s\",\"password\":\"%s\"}", name, password);

    int status = http_post("/won/login", body, resp, sizeof(resp));

    if (status == 200 && json_get_bool(resp, "ok")) {
        strncpy(g_username, name, sizeof(g_username) - 1);
        json_get_str(resp, "token", g_token, sizeof(g_token));
        g_logged_in = TRUE;
        if (authH) *authH = WON_AUTH_HANDLE;
        return WON_STATUS_SUCCESS;
    }
    if (status == 401) return WON_STATUS_BADPASSWORD;
    if (status == 404) return WON_STATUS_BADUSER;
    return WON_STATUS_FAILURE;
}

/*
 * WONAuthLoginNewAccountA — create account then log in.
 */
__declspec(dllexport) int WINAPI WONAuthLoginNewAccountA(
    void   *servers,
    DWORD   nServers,
    const char *name,
    const char *password,
    long    timeout,
    HANDLE *authH,
    HANDLE *peerKeyH)
{
    (void)servers; (void)nServers; (void)timeout; (void)peerKeyH;

    if (!name || !password) return WON_STATUS_FAILURE;

    char body[256], resp[512];
    snprintf(body, sizeof(body),
             "{\"name\":\"%s\",\"password\":\"%s\"}", name, password);

    int status = http_post("/won/create-account", body, resp, sizeof(resp));

    if (status == 200 || status == 201) {
        if (json_get_bool(resp, "ok")) {
            strncpy(g_username, name, sizeof(g_username) - 1);
            json_get_str(resp, "token", g_token, sizeof(g_token));
            g_logged_in = TRUE;
            if (authH) *authH = WON_AUTH_HANDLE;
            return WON_STATUS_SUCCESS;
        }
    }
    if (status == 409) return WON_STATUS_FAILURE; /* name taken */
    return WON_STATUS_FAILURE;
}

/*
 * WONAuthGetNicknameA — copy the logged-in user's name into buf.
 */
__declspec(dllexport) int WINAPI WONAuthGetNicknameA(
    HANDLE authH,
    char  *buf,
    DWORD  buflen)
{
    (void)authH;
    if (!buf || buflen == 0) return WON_STATUS_FAILURE;
    if (!g_logged_in)        return WON_STATUS_FAILURE;
    strncpy(buf, g_username, buflen - 1);
    buf[buflen - 1] = '\0';
    return WON_STATUS_SUCCESS;
}

/* Stub — the game uses kver.pub for certificate verification.
 * We skip verification; just report success. */
__declspec(dllexport) int WINAPI WONAuthLoadVerifierKeyFromFileA(
    const char *path)
{
    (void)path;
    return WON_STATUS_SUCCESS;
}

/* Return a dummy certificate blob. */
__declspec(dllexport) int WINAPI WONAuthGetCertificate(
    HANDLE authH,
    void  *buf,
    DWORD *buflen)
{
    (void)authH;
    if (buflen) *buflen = 0;
    return WON_STATUS_SUCCESS;
}

/* Return a dummy private key blob. */
__declspec(dllexport) int WINAPI WONAuthGetPrivateKey(
    HANDLE authH,
    void  *buf,
    DWORD *buflen)
{
    (void)authH;
    if (buflen) *buflen = 0;
    return WON_STATUS_SUCCESS;
}

__declspec(dllexport) void WINAPI WONAuthCloseHandle(HANDLE authH)
{
    (void)authH;
    /* On explicit logout, clear session. */
    g_logged_in = FALSE;
    g_username[0] = '\0';
    g_token[0]    = '\0';
}

/* =========================================================================
 * IP ADDRESS
 * ======================================================================= */

/*
 * WONIPAddressSetFromString — parse "host:port" into a WONIPAddress struct.
 * The struct layout (reconstructed) is { DWORD addr; WORD port; }.
 */
__declspec(dllexport) int WINAPI WONIPAddressSetFromString(
    void       *addr_out,   /* WONIPAddress* */
    const char *str)
{
    if (!addr_out || !str) return WON_STATUS_FAILURE;
    /* Parse host:port and write as { DWORD ip; WORD port; } */
    char host[128]; WORD port = 517;
    const char *colon = strrchr(str, ':');
    if (colon) {
        int hlen = (int)(colon - str);
        strncpy(host, str, hlen); host[hlen] = '\0';
        port = (WORD)atoi(colon + 1);
    } else {
        strncpy(host, str, sizeof(host) - 1);
    }
    struct in_addr ia;
    ia.s_addr = inet_addr(host);
    DWORD *dw = (DWORD *)addr_out;
    dw[0] = ia.s_addr;
    ((WORD *)(dw + 1))[0] = htons(port);
    return WON_STATUS_SUCCESS;
}

/* =========================================================================
 * PROFILE FUNCTIONS
 * Profile fields the original game reads/writes (from executable strings):
 *   AGENT, OF, LEVEL, CREDITS, MISSIONS, VICTORIES, FORFEITS, REPUTE
 * We store these server-side; the game fetches/sets them by string key.
 * ======================================================================= */

__declspec(dllexport) HANDLE WINAPI WONProfileCreate(HANDLE authH)
{
    (void)authH;
    return g_logged_in ? WON_PROFILE_HANDLE : NULL;
}

__declspec(dllexport) void WINAPI WONProfileCloseHandle(HANDLE profileH)
{
    (void)profileH;
}

/*
 * WONProfileGet — fetch one profile field from the server.
 *
 * Original signature (reconstructed):
 *   WONError WONProfileGet(HANDLE profile, const char *key,
 *                          char *buf, DWORD buflen);
 */
__declspec(dllexport) int WINAPI WONProfileGet(
    HANDLE      profileH,
    const char *key,
    char       *buf,
    DWORD       buflen)
{
    (void)profileH;
    if (!g_logged_in || !key || !buf) return WON_STATUS_FAILURE;

    char path[256], resp[1024];
    snprintf(path, sizeof(path), "/won/profile/%s", g_username);
    int status = http_get(path, resp, sizeof(resp));

    if (status != 200) return WON_STATUS_FAILURE;

    /* Key names the game uses match our JSON field names. */
    char val[128] = {0};
    if (!json_get_str(resp, key, val, sizeof(val)))
        return WON_STATUS_FAILURE;

    strncpy(buf, val, buflen - 1);
    buf[buflen - 1] = '\0';
    return WON_STATUS_SUCCESS;
}

/*
 * WONProfileSet — write one profile field to the server.
 */
__declspec(dllexport) int WINAPI WONProfileSet(
    HANDLE      profileH,
    const char *key,
    const char *value)
{
    (void)profileH;
    if (!g_logged_in || !key || !value) return WON_STATUS_FAILURE;

    char path[256], body[512], resp[128];
    snprintf(path, sizeof(path), "/won/profile/%s", g_username);
    snprintf(body, sizeof(body),
             "{\"key\":\"%s\",\"value\":\"%s\",\"token\":\"%s\"}",
             key, value, g_token);

    int status = http_post(path, body, resp, sizeof(resp));
    return (status == 200) ? WON_STATUS_SUCCESS : WON_STATUS_FAILURE;
}

/*
 * WONProfileRemove — delete a profile field.
 */
__declspec(dllexport) int WINAPI WONProfileRemove(
    HANDLE      profileH,
    const char *key)
{
    (void)profileH; (void)key;
    /* Not needed for Silencer — stub success. */
    return WON_STATUS_SUCCESS;
}

/*
 * WONProfileCreateAccount — called once after account creation to initialise
 * the profile. The server already creates a default profile on /won/create-account.
 */
__declspec(dllexport) int WINAPI WONProfileCreateAccount(
    HANDLE      profileH,
    const char *name,
    const char *password)
{
    (void)profileH; (void)name; (void)password;
    return WON_STATUS_SUCCESS;
}

/*
 * WONProfileGetAccount — fetch account-level info (name, email, etc).
 * Silencer only uses the nickname, which WONAuthGetNicknameA already provides.
 */
__declspec(dllexport) int WINAPI WONProfileGetAccount(
    HANDLE  profileH,
    char   *nameBuf,
    DWORD   nameLen,
    char   *emailBuf,
    DWORD   emailLen)
{
    (void)profileH;
    if (nameBuf && nameLen > 0) {
        strncpy(nameBuf, g_username, nameLen - 1);
        nameBuf[nameLen - 1] = '\0';
    }
    if (emailBuf && emailLen > 0) emailBuf[0] = '\0';
    return WON_STATUS_SUCCESS;
}

/*
 * WONProfileUpdateAccount — change password / email. Not used by Silencer
 * during normal play.
 */
__declspec(dllexport) int WINAPI WONProfileUpdateAccount(
    HANDLE      profileH,
    const char *name,
    const char *password,
    const char *email)
{
    (void)profileH; (void)name; (void)password; (void)email;
    return WON_STATUS_SUCCESS;
}
