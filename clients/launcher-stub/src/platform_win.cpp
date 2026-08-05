// Windows backend: WinHTTP for HTTP(S), TaskDialogIndirect for the progress
// window (comctl32 v6 via stub.manifest), CreateProcess for spawn, and
// System32\tar.exe (bsdtar) for zip extraction. OS facilities only - the
// stub ships no DLLs.

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <commctrl.h>
#include <winhttp.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "stub.h"

namespace {

std::wstring widen(const std::string &s) {
  if (s.empty())
    return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  std::wstring w(n, 0);
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
  return w;
}

std::string narrow(const std::wstring &w) {
  if (w.empty())
    return "";
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0,
                              nullptr, nullptr);
  std::string s(n, 0);
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr,
                      nullptr);
  return s;
}

} // namespace

std::string stub::env_str(const char *name) {
  wchar_t buf[4096];
  DWORD n = GetEnvironmentVariableW(widen(name).c_str(), buf, 4096);
  if (n == 0 || n >= 4096)
    return "";
  return narrow(std::wstring(buf, n));
}

std::string stub::own_exe_path() {
  wchar_t buf[4096];
  DWORD n = GetModuleFileNameW(nullptr, buf, 4096);
  return narrow(std::wstring(buf, n));
}

// ---------------- HTTP ----------------
namespace {

struct Url {
  std::wstring host, path;
  INTERNET_PORT port = 0;
  bool https = false;
  bool ok = false;
};

Url crack(const std::string &url) {
  Url u;
  std::wstring w = widen(url);
  URL_COMPONENTS c{};
  c.dwStructSize = sizeof c;
  c.dwSchemeLength = (DWORD)-1;
  c.dwHostNameLength = (DWORD)-1;
  c.dwUrlPathLength = (DWORD)-1;
  c.dwExtraInfoLength = (DWORD)-1;
  if (!WinHttpCrackUrl(w.c_str(), (DWORD)w.size(), 0, &c))
    return u;
  u.host.assign(c.lpszHostName, c.dwHostNameLength);
  u.path.assign(c.lpszUrlPath, c.dwUrlPathLength);
  if (c.dwExtraInfoLength)
    u.path.append(c.lpszExtraInfo, c.dwExtraInfoLength);
  u.port = c.nPort;
  u.https = c.nScheme == INTERNET_SCHEME_HTTPS;
  u.ok = true;
  return u;
}

// timeout_ms > 0 bounds each request stage (manifest); 0 means "no total
// bound, 30s stall timeout" (payload download). WinHTTP follows redirects
// itself and refuses https->http downgrades by default.
stub::HttpResult http_fetch(const std::string &url, int timeout_ms, FILE *out_file,
                            std::string *out_mem, const stub::DlProgress &progress) {
  stub::HttpResult res;
  Url u = crack(url);
  if (!u.ok) {
    res.error = "bad url";
    return res;
  }
  HINTERNET ses = WinHttpOpen(L"silencer-launcher-stub",
                              WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!ses) {
    res.error = "WinHttpOpen failed";
    return res;
  }
  int t = timeout_ms > 0 ? timeout_ms : 30000;
  WinHttpSetTimeouts(ses, t, t, t, t);
  HINTERNET con = WinHttpConnect(ses, u.host.c_str(), u.port, 0);
  HINTERNET req = con ? WinHttpOpenRequest(con, L"GET", u.path.c_str(), nullptr,
                                           WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           u.https ? WINHTTP_FLAG_SECURE : 0)
                      : nullptr;
  bool ok = req &&
            WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(req, nullptr);
  if (!ok) {
    res.error = "request failed (err " + std::to_string(GetLastError()) + ")";
  } else {
    DWORD status = 0, sl = sizeof status;
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &sl,
                        WINHTTP_NO_HEADER_INDEX);
    if (status >= 400) {
      ok = false;
      res.error = "http " + std::to_string(status);
    }
  }
  uint64_t total = 0;
  if (ok) {
    wchar_t cl[64];
    DWORD cll = sizeof cl;
    if (WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH,
                            WINHTTP_HEADER_NAME_BY_INDEX, cl, &cll,
                            WINHTTP_NO_HEADER_INDEX))
      total = (uint64_t)_wtoi64(cl);
  }
  uint64_t got = 0;
  std::vector<char> buf(64 * 1024);
  const ULONGLONG start = GetTickCount64();
  while (ok) {
    // WinHttpSetTimeouts only bounds each stage/read, so a trickling server
    // would keep the loop alive forever; enforce the total deadline here.
    if (timeout_ms > 0 && GetTickCount64() - start > (ULONGLONG)timeout_ms) {
      ok = false;
      res.error = "timed out";
      break;
    }
    DWORD n = 0;
    if (!WinHttpReadData(req, buf.data(), (DWORD)buf.size(), &n)) {
      ok = false;
      res.error = "read failed";
      break;
    }
    if (n == 0)
      break;
    if (out_mem) {
      out_mem->append(buf.data(), n);
      if (out_mem->size() > 1024 * 1024) {
        ok = false;
        res.error = "response too large";
        break;
      }
    }
    if (out_file && fwrite(buf.data(), 1, n, out_file) != n) {
      ok = false;
      res.error = "write failed";
      break;
    }
    got += n;
    if (progress && !progress(got, total)) {
      ok = false;
      res.error = "cancelled";
      break;
    }
  }
  if (req)
    WinHttpCloseHandle(req);
  if (con)
    WinHttpCloseHandle(con);
  WinHttpCloseHandle(ses);
  res.ok = ok;
  return res;
}

} // namespace

stub::HttpResult stub::http_get_text(const std::string &url, int timeout_ms,
                                     std::string *out, const DlProgress &progress) {
  out->clear();
  return http_fetch(url, timeout_ms, nullptr, out, progress);
}

stub::HttpResult stub::http_download(const std::string &url, const std::string &dest,
                                     const DlProgress &progress) {
  FILE *f = _wfopen(widen(dest).c_str(), L"wb");
  if (!f)
    return {false, "cannot open " + dest};
  HttpResult r = http_fetch(url, 0, f, nullptr, progress);
  fclose(f);
  return r;
}

// ---------------- GUI ----------------
namespace {

struct GuiCtx {
  stub::GuiState *st;
  bool downloading_ui = false;
  bool closing = false;
};

HRESULT CALLBACK td_cb(HWND hwnd, UINT msg, WPARAM wp, LPARAM, LONG_PTR ref) {
  GuiCtx *ctx = (GuiCtx *)ref;
  stub::GuiState &st = *ctx->st;
  switch (msg) {
  case TDN_CREATED:
    SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 30);
    break;
  case TDN_TIMER:
    if (st.done.load()) {
      ctx->closing = true;
      SendMessageW(hwnd, TDM_CLICK_BUTTON, IDCANCEL, 0);
      break;
    }
    if (!ctx->downloading_ui && st.phase.load() == stub::kDownloading) {
      ctx->downloading_ui = true;
      SendMessageW(hwnd, TDM_SET_MARQUEE_PROGRESS_BAR, FALSE, 0);
      SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_RANGE, 0, MAKELPARAM(0, 100));
      SendMessageW(hwnd, TDM_SET_ELEMENT_TEXT, TDE_MAIN_INSTRUCTION,
                   (LPARAM)L"Updating...");
    }
    if (ctx->downloading_ui) {
      int pc = st.percent.load();
      if (pc >= 0)
        SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_POS, pc, 0);
    }
    break;
  case TDN_BUTTON_CLICKED:
    if ((int)wp == IDCANCEL && !ctx->closing) {
      st.cancel = true; // skip the update; the dialog closes when the
      return S_FALSE;   // worker finishes and sets done
    }
    break;
  }
  return S_OK;
}

} // namespace

void stub::gui_run(GuiState &st) {
  if (env_str("SILENCER_STUB_NO_GUI") == "1") {
    while (!st.done.load())
      Sleep(30);
    return;
  }
  GuiCtx ctx{&st};
  TASKDIALOGCONFIG cfg{};
  cfg.cbSize = sizeof cfg;
  cfg.dwFlags = TDF_CALLBACK_TIMER | TDF_SHOW_MARQUEE_PROGRESS_BAR |
                TDF_ALLOW_DIALOG_CANCELLATION;
  cfg.dwCommonButtons = TDCBF_CANCEL_BUTTON;
  cfg.pszWindowTitle = L"Silencer Launcher";
  cfg.pszMainInstruction = L"Checking for updates...";
  cfg.pfCallback = td_cb;
  cfg.lpCallbackData = (LONG_PTR)&ctx;
  if (FAILED(TaskDialogIndirect(&cfg, nullptr, nullptr, nullptr))) {
    logf("TaskDialogIndirect failed - continuing without a window");
    while (!st.done.load()) // comctl32 v6 unavailable: behave as NO_GUI
      Sleep(30);
  }
}

void stub::error_box(const std::string &msg) {
  logf("%s", msg.c_str());
  if (env_str("SILENCER_STUB_NO_GUI") == "1")
    return;
  MessageBoxW(nullptr, widen(msg).c_str(), L"Silencer Launcher",
              MB_ICONERROR | MB_OK);
}

// ---------------- processes ----------------
bool stub::spawn(const std::string &exe, const std::string &workdir, Child *out) {
  std::wstring wexe = widen(exe);
  std::wstring wdir = widen(workdir);
  std::wstring cmd = L"\"" + wexe + L"\"";
  std::vector<wchar_t> cmdbuf(cmd.begin(), cmd.end());
  cmdbuf.push_back(0);
  STARTUPINFOW si{};
  si.cb = sizeof si;
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(wexe.c_str(), cmdbuf.data(), nullptr, nullptr, FALSE, 0,
                      nullptr, wdir.empty() ? nullptr : wdir.c_str(), &si, &pi))
    return false;
  CloseHandle(pi.hThread);
  out->handle = (intptr_t)pi.hProcess;
  return true;
}

int stub::try_wait(const Child &c, int timeout_ms, bool *exited) {
  *exited = false;
  if (!c.valid())
    return -1;
  HANDLE h = (HANDLE)c.handle;
  if (WaitForSingleObject(h, timeout_ms) != WAIT_OBJECT_0)
    return -1;
  DWORD code = 0;
  GetExitCodeProcess(h, &code);
  *exited = true;
  return (int)code; // crash exits are negative NTSTATUS values - still nonzero
}

bool stub::run_tool(const std::vector<std::string> &argv) {
  std::wstring cmd;
  for (auto &a : argv) {
    if (!cmd.empty())
      cmd += L" ";
    cmd += L"\"" + widen(a) + L"\"";
  }
  std::vector<wchar_t> buf(cmd.begin(), cmd.end());
  buf.push_back(0);
  STARTUPINFOW si{};
  si.cb = sizeof si;
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(widen(argv[0]).c_str(), buf.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    return false;
  CloseHandle(pi.hThread);
  bool done = WaitForSingleObject(pi.hProcess, 120000) == WAIT_OBJECT_0;
  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  if (!done)
    TerminateProcess(pi.hProcess, 1);
  CloseHandle(pi.hProcess);
  return done && code == 0;
}

bool stub::verify_payload(const std::string &) {
  return true; // sha256 already verified; no Authenticode signing (yet)
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) { return stub_main(); }
