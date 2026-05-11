#include "lobby_shell.h"

#include "context.h"

#include "layout.h"
#include "lobby_character.h"
#include "lobby_chat.h"
#include "lobby_create.h"
#include "lobby_join.h"
#include "lobby_select.h"
#include "lobby_tech.h"
#include "node.h"
#include "render.h"

#include "ambience_mixer.h"
#include "buyableitem.h"
#include "config.h"
#include "game.h"
#include "game_state.h"
#include "lobby.h"
#include "lobbygame.h"
#include "map_downloader.h"
#include "mapfetch.h"
#include "os.h"
#include "peer.h"
#include "screen_context.h"
#include "serializer.h"
#include "sha1.h"
#include "team.h"
#include "user.h"
#include "world.h"
#include "renderer.h"
#include "surface.h"

#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace ui {
namespace v2 {

Node BuildLobby(const Context & ctx, const LobbyHandlers & handlers, const LobbyState & state)
{
	// Mirrors LobbyScreen::Build (clients/silencer/src/ui/screens/lobby/
	// lobby_screen.cpp). Sub-panels are ported one at a time — character
	// landed with P10, chat with P11, game_create with P12. Other
	// right-side panels (game_select/game_join/game_tech) follow in
	// P13–P15. The map-name overlay (uid 8) has empty text at preview
	// gate and renders nothing, so it is omitted here.
	std::string version_label = "v.";
	if(ctx.version) version_label += ctx.version;
	// Chat caret is ON only when the chat sub-interface is the lobby's
	// active object. Legacy ShowGameCreate / ShowGameTech clear
	// chatiface->activeobject + re-run ActiveChanged → caret flips off.
	// ShowGameJoin / ShowGameSelect do NOT touch chatiface->activeobject
	// (lobby_screen.cpp 328-335 / 355-363), so the chat caret stays lit
	// while those panels are up.
	const bool chat_active = (state.active_panel == LobbyActivePanel::None ||
	                          state.active_panel == LobbyActivePanel::GameJoin ||
	                          state.active_panel == LobbyActivePanel::GameSelect);

	std::vector<Node> children = {
		Label("Silencer", /*font_bank=*/135, /*font_width=*/11)
			.at(15, 32)
			.withColor(152),
		Label(version_label, /*font_bank=*/133, /*font_width=*/6)
			.at(115, 39)
			.withColor(189),
		Button("Go Back", ButtonType::B156x21)
			.at(473, 29)
			.onClick(handlers.on_go_back),
		BuildCharacterPanel(ctx, state.character),
		BuildChatPanel(ctx, state.chat, chat_active),
	};
	// Map-name overlay (uid 8 in the legacy). Empty at preview gate /
	// preview-gate state — emit only when the live engine has filled it
	// in (after CONNECTED → game-join handoff).
	if(!state.map_name.empty()){
		children.push_back(
			Label(state.map_name, /*font_bank=*/135, /*font_width=*/11)
				.at(180, 32)
				.withColor(129)
				.withBrightness(128 + 32)
				.withRamp());
	}
	if(state.active_panel == LobbyActivePanel::GameCreate){
		children.push_back(BuildGameCreatePanel(ctx, state.game_create, handlers.game_create));
	}else if(state.active_panel == LobbyActivePanel::GameJoin){
		children.push_back(BuildGameJoinPanel(ctx, state.game_join, handlers.game_join));
	}else if(state.active_panel == LobbyActivePanel::GameSelect){
		children.push_back(BuildGameSelectPanel(ctx, state.game_select, handlers.game_select));
	}else if(state.active_panel == LobbyActivePanel::GameTech){
		children.push_back(BuildGameTechPanel(ctx, state.game_tech, handlers.game_tech));
	}
	return Background(/*bank=*/7, /*index=*/1, std::move(children));
}

// -----------------------------------------------------------------------------
// LobbyRuntime — engine wire-in for GameState::LOBBY.
// -----------------------------------------------------------------------------

namespace {

LobbyHandlers BuildLobbyHandlers(LobbyRuntime * self){
	LobbyHandlers h;
	h.on_go_back = [self](){ self->HandleBack(); };
	h.character.on_select_agency = [self](Uint8 agency){ self->SelectAgency(agency); };
	h.game_select.on_create = [self](){ self->ShowGameCreate(); };
	h.game_select.on_join   = [self](){ self->GameSelectJoin(); };
	h.game_create.on_security_toggle = [self](){ self->CreateCycleSecurity(); };
	h.game_create.on_create          = [self](){ self->CreateGame(); };
	h.game_join.on_ready             = [self](){ self->GameJoinReady(); };
	h.game_join.on_change_team       = [self](){ self->GameJoinChangeTeam(); };
	h.game_join.on_choose_tech       = [self](){ self->ShowGameTech(); };
	h.game_tech.on_back_to_teams     = [self](){ self->ShowGameJoin(); };
	h.game_tech.on_toggle_tech       = [self](int idx){ self->GameTechToggle(idx); };
	h.game_tech.on_select_tech       = [self](int idx){ self->GameTechSelect(idx); };
	return h;
}

void CycleNumeric(std::string & dst, int lo, int hi){
	int v = std::atoi(dst.c_str());
	v = (v >= hi) ? lo : (v + 1);
	dst = std::to_string(v);
}

}  // namespace

LobbyRuntime::LobbyRuntime(World & world, ScreenContext & sctx, Game & game)
	: world_(world), sctx_(sctx), game_(game) {
	state_data_.active_panel       = LobbyActivePanel::GameSelect;
	state_data_.character.selected_agency = Config::GetInstance().defaultagency;
}

bool LobbyRuntime::ChatActive() const {
	const LobbyActivePanel p = state_data_.active_panel;
	return p == LobbyActivePanel::None ||
	       p == LobbyActivePanel::GameJoin ||
	       p == LobbyActivePanel::GameSelect;
}

bool LobbyRuntime::CreateInputActive() const {
	return state_data_.active_panel == LobbyActivePanel::GameCreate &&
	       create_active_field_ > 0;
}

void LobbyRuntime::RefreshCharacter(){
	CharacterPanelState & cs = state_data_.character;
	cs.username = world_.lobby.GetLocalUsername();
	User * user = world_.lobby.GetUserInfo(world_.lobby.accountid);
	if(user && !user->retrieving){
		Uint8 a = cs.selected_agency;
		cs.level_text  = "LEVEL: "  + std::to_string(user->agency[a].level);
		cs.wins_text   = "WINS: "   + std::to_string(user->agency[a].wins);
		cs.losses_text = "LOSSES: " + std::to_string(user->agency[a].losses);
		int lvl = user->agency[a].level;
		int remaining = 100 * (lvl + 1) - (int)user->agency[a].xptonextlevel;
		cs.xp_text = "XP TO NEXT LEVEL: " + std::to_string(remaining);
	}else{
		cs.level_text.clear();
		cs.wins_text.clear();
		cs.losses_text.clear();
		cs.xp_text.clear();
	}
}

void LobbyRuntime::RefreshChat(){
	ChatPanelState & cs = state_data_.chat;

	// Channel-name overlay.
	if(world_.lobby.channelchanged){
		if(world_.lobby.lastchannel[0] == '\0'){
			std::strcpy(world_.lobby.lastchannel, world_.lobby.channel);
		}
		cs.channel_name = world_.lobby.channel;
		world_.lobby.channelchanged = false;
	}

	// Drain incoming chat into the scrollback.
	bool any = false;
	while(!world_.lobby.chatmessages.empty()){
		auto & message = world_.lobby.chatmessages.front();
		const char * text = message.data();
		size_t tlen = std::strlen(text);
		Uint8 color = (Uint8)message[tlen + 1];
		Uint8 brightness = (Uint8)message[tlen + 2];
		const size_t maxchars = 242 / 6;
		std::string s(text, std::min(tlen, maxchars));
		ChatLine cl{std::move(s), color, brightness};
		cs.chat_lines.push_back(std::move(cl));
		if(cs.chat_lines.size() > 256){
			cs.chat_lines.pop_front();
		}
		world_.lobby.chatmessages.pop_front();
		any = true;
	}
	if(any){
		const Uint16 visible = 207 / 11;
		if(cs.chat_lines.size() > visible){
			cs.chat_scrolled = (Uint16)(cs.chat_lines.size() - visible);
		}else{
			cs.chat_scrolled = 0;
		}
	}

	// Presence list — rebuild on change.
	if(world_.lobby.presencechanged || !world_.lobby.gamesprocessed){
		cs.presence_lines.clear();
		struct Row { Uint8 group; std::string label; };
		std::vector<Row> rows;
		rows.reserve(world_.lobby.presence.size());
		for(auto & kv : world_.lobby.presence){
			Lobby::PresenceEntry & e = kv.second;
			Row r;
			r.label = e.name;
			r.group = (e.status <= 2) ? e.status : 0;
			if(e.gameid != 0){
				LobbyGame * g = world_.lobby.GetGameById(e.gameid);
				if(g){
					r.label += " [";
					r.label += g->name;
					r.label += "]";
				}
			}
			rows.push_back(std::move(r));
		}
		std::sort(rows.begin(), rows.end(), [](const Row & a, const Row & b){
			if(a.group != b.group) return a.group < b.group;
			return a.label < b.label;
		});
		const size_t maxchars = 110 / 6;
		Uint8 lastgroup = 255;
		for(auto & r : rows){
			if(r.group != lastgroup){
				const char * header = (r.group == 0) ? "In Lobby" : (r.group == 1) ? "Pregame" : "Playing";
				ChatLine cl;
				cl.text = header;
				cl.color = 0;
				cl.brightness = 128 + 32;
				cs.presence_lines.push_back(std::move(cl));
				lastgroup = r.group;
			}
			ChatLine cl;
			cl.text = "  " + r.label;
			if(cl.text.size() > maxchars) cl.text.resize(maxchars);
			cl.color = 0;
			cl.brightness = 128;
			cs.presence_lines.push_back(std::move(cl));
		}
		world_.lobby.presencechanged = false;
	}

	// Caret blink.
	cs.caret_visible = ((Uint32)game_.GetFrameCount() & 31u) < 16u;
}

void LobbyRuntime::RefreshGameSelect(){
	GameSelectState & gs = state_data_.game_select;
	const int max_visible_rows = 265 / 14;

	if(!world_.lobby.gamesprocessed){
		size_t write = 0;
		int new_selected = -1;
		Uint32 selected_id = (gs.selected_index >= 0 && gs.selected_index < (int)gs.games.size())
			? gs.games[gs.selected_index].id : 0;
		for(size_t r = 0; r < gs.games.size(); r++){
			if(world_.lobby.GetGameById(gs.games[r].id)){
				if(write != r) gs.games[write] = std::move(gs.games[r]);
				if(gs.games[write].id == selected_id) new_selected = (int)write;
				write++;
			}
		}
		gs.games.resize(write);
		gs.selected_index = new_selected;

		for(LobbyGame * lobbygame : world_.lobby.games){
			bool found = false;
			for(auto & row : gs.games){
				if(row.id == lobbygame->id){ found = true; break; }
			}
			if(found) continue;
			GameSelectGame row;
			row.id = lobbygame->id;
			row.name = lobbygame->name;
			row.passworded = (std::strlen(lobbygame->password) > 0);
			row.accountid = lobbygame->accountid;
			row.mapname = lobbygame->mapname;
			User * creator = world_.lobby.GetUserInfo(lobbygame->accountid);
			if(creator) row.creator_name = creator->name;
			row.securitylevel = lobbygame->securitylevel;
			row.minlevel = lobbygame->minlevel;
			row.maxlevel = lobbygame->maxlevel;
			row.maxplayers = lobbygame->maxplayers;
			row.maxteams = lobbygame->maxteams;
			gs.games.push_back(std::move(row));
		}

		world_.lobby.gamesprocessed = true;
	}else{
		for(auto & row : gs.games){
			if(!row.creator_name.empty()) continue;
			User * creator = world_.lobby.GetUserInfo(row.accountid);
			if(creator) row.creator_name = creator->name;
		}
	}

	if(gs.selected_index >= (int)gs.games.size()) gs.selected_index = -1;
	gs.show_scrollbar = (int)gs.games.size() > max_visible_rows;
	const int scrollmax = std::max(0, (int)gs.games.size() - max_visible_rows);
	if(gs.scrolled > scrollmax) gs.scrolled = (Uint16)scrollmax;
}

void LobbyRuntime::RefreshGameCreate(){
	GameCreateState & gcs = state_data_.game_create;
	gcs.caret_visible = ((Uint32)game_.GetFrameCount() & 31u) < 16u;

	if(!sctx_.mapDownloader.dlitemname.empty()){
		int res = sctx_.mapDownloader.dlresult.load();
		if(res == 1){
			std::string completed = sctx_.mapDownloader.dlitemname;
			sctx_.mapDownloader.servermaps.erase(completed);
			gcs.server_maps.erase(completed);
			for(auto & item : gcs.map_items){
				if(item == completed){
					if(item.size() > 5) item = item.substr(5);
					break;
				}
			}
			gcs.download_item.clear();
			gcs.download_progress = -1;
			sctx_.mapDownloader.dlitemname.clear();
		}else if(res == -1){
			gcs.download_item.clear();
			gcs.download_progress = -1;
			sctx_.mapDownloader.dlitemname.clear();
			game_.ShowV2Message("Download failed");
		}else{
			gcs.download_progress = sctx_.mapDownloader.dlprogress.load();
		}
	}
}

void LobbyRuntime::RefreshGameJoin(){
	if(state_data_.active_panel != LobbyActivePanel::GameJoin) return;
	if(world_.gameplaystate != World::INLOBBY) return;
	Peer * localpeer = world_.peerlist[world_.localpeerid];
	bool blocked = localpeer && localpeer->ishost && !world_.AllPeersDownloadedMap();
	state_data_.game_join.waiting = blocked;
}

void LobbyRuntime::RefreshGameTech(){
	if(state_data_.active_panel != LobbyActivePanel::GameTech) return;
	GameTechState & gs = state_data_.game_tech;

	int techslotsleft = 0;
	Peer * localpeer = world_.peerlist[world_.localpeerid];
	Team * team = world_.GetPeerTeam(world_.localpeerid);

	if(localpeer){
		User * user = world_.lobby.GetUserInfo(localpeer->accountid);
		if(user && team){
			techslotsleft = user->agency[team->agency].techslots - world_.TechSlotsUsed(*localpeer);
			gs.slots_left_text = "Tech slots left: " + std::to_string(techslotsleft);
		}else{
			gs.slots_left_text.clear();
		}
	}else{
		gs.slots_left_text.clear();
		if(world_.tickcount % 12 == 0){
			world_.RequestPeerList();
		}
	}

	if(!team){
		gs.items.clear();
		for(int i = 0; i < 3; i++) gs.peer_cols[i] = GameTechPeerCol{};
		return;
	}

	struct VisibleItem { BuyableItem * item; int local_button_idx; };
	std::vector<VisibleItem> visible;
	visible.reserve(world_.buyableitems.size());
	int b = 0;
	for(auto * buyableitem : world_.buyableitems){
		if(buyableitem->techslots){
			if(buyableitem->agencyspecific == -1 || buyableitem->agencyspecific == team->agency){
				visible.push_back({buyableitem, b});
			}
			b++;
		}
	}

	gs.items.assign(visible.size(), GameTechItem{});

	for(size_t r = 0; r < visible.size(); r++){
		BuyableItem * buyableitem = visible[r].item;
		gs.items[r].label = std::string(buyableitem->name) + " (" +
		                    std::to_string(buyableitem->techslots) + ")";
	}

	int peerindex = 0;
	for(int i = 0; i < 4; i++){
		Peer * peer = world_.peerlist[team->peers[i]];
		User * user = peer ? world_.lobby.GetUserInfo(peer->accountid) : nullptr;
		bool draw = (i < team->numpeers);
		bool is_local = (team->peers[i] == world_.localpeerid);
		int col = is_local ? 3 : peerindex;
		if(!is_local && peerindex < 3){
			GameTechPeerCol & ph = gs.peer_cols[peerindex];
			ph.draw = draw;
			if(draw && user){
				ph.name = user->name;
				ph.name_x = (Sint16)(375 - (ph.name.length() * 6));
			}else{
				ph.name.clear();
				ph.name_x = 0;
			}
		}
		for(size_t r = 0; r < visible.size(); r++){
			BuyableItem * buyableitem = visible[r].item;
			GameTechItem & row = gs.items[r];
			row.draw[col]    = draw;
			bool checked = peer && (peer->techchoices & buyableitem->techchoice);
			row.checked[col] = checked;
			if(is_local){
				bool usable = true;
				if(buyableitem->techslots <= techslotsleft || checked){
					usable = true;
				}else{
					usable = false;
				}
				row.local_usable = usable;
				row.bright[col]  = usable ? (Uint8)64 : (Uint8)128;
			}else{
				row.bright[col] = 64;
			}
		}
		if(!is_local){
			peerindex++;
		}
	}
}

void LobbyRuntime::Render(Surface & target, ::Renderer & renderer,
                          int mouse_x, int mouse_y, float dt,
                              int logical_w, int logical_h, int scale){
	Context ctx{
		world_.resources,
		/*logical_w=*/logical_w,
		/*logical_h=*/logical_h,
		/*scale=*/scale,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = mouse_x;
	ctx.mouse_y = mouse_y;
	ctx.state   = &state_;
	ctx.dt      = dt;

	RefreshCharacter();
	RefreshChat();
	RefreshGameSelect();
	RefreshGameCreate();
	RefreshGameJoin();
	RefreshGameTech();
	LobbyHandlers handlers = BuildLobbyHandlers(this);
	target.Clear(0);
	state_.BeginFrame();
	Node tree = BuildLobby(ctx, handlers, state_data_);
	::ui::v2::EnsureClayContext(ctx);
	Clay_SetPointerState(Clay_Vector2{ (float)mouse_x, (float)mouse_y }, /*pointer_down=*/false);
	Clay_UpdateScrollContainers(/*drag=*/false, Clay_Vector2{ 0.0f, 0.0f }, dt);
	Layout(tree, ctx);
	::ui::v2::Render(tree, ctx, target, renderer);
	state_.EndFrame();
}

bool LobbyRuntime::DispatchMouseDown(int mouse_x, int mouse_y,
                                int logical_w, int logical_h, int scale){
	// Character-panel agency toggles. Bank 181, indices 0..4 at (20 + i*42, 90).
	for(int i = 0; i < 5; i++){
		Sint16 anchor_x = (Sint16)(20 + i * 42);
		Sint16 anchor_y = 90;
		Uint8 idx = (Uint8)i;
		int x1 = anchor_x - world_.resources.spriteoffsetx[181][idx];
		int y1 = anchor_y - world_.resources.spriteoffsety[181][idx];
		int x2 = x1 + world_.resources.spritewidth[181][idx];
		int y2 = y1 + world_.resources.spriteheight[181][idx];
		if(mouse_x >= x1 && mouse_x < x2 && mouse_y >= y1 && mouse_y < y2){
			SelectAgency(idx);
			return true;
		}
	}

	// Game-select SelectBox row hit-test.
	if(state_data_.active_panel == LobbyActivePanel::GameSelect){
		const GameSelectState & gs = state_data_.game_select;
		if(mouse_x >= 407 && mouse_x < 407 + 214 - 16 &&
		   mouse_y >= 89  && mouse_y < 89 + 265){
			int idx = ((mouse_y - 89) / 14) + gs.scrolled;
			if(idx >= 0 && idx < (int)gs.games.size()){
				GameSelectRow(idx);
				return true;
			}
		}
	}

	// Game-create panel hit-tests.
	if(state_data_.active_panel == LobbyActivePanel::GameCreate){
		const int row_y0 = 93;
		const int row_dy = 18;
		if(mouse_x >= 323 && mouse_x < 323 + 70 &&
		   mouse_y >= row_y0 + 0 * row_dy && mouse_y < row_y0 + 0 * row_dy + 20){
			CreateCycleSecurity();
			return true;
		}
		auto numeric_row_hit = [&](int row){
			return mouse_x >= 350 && mouse_x < 350 + 20 &&
			       mouse_y >= row_y0 + row * row_dy &&
			       mouse_y < row_y0 + row * row_dy + 20;
		};
		if(numeric_row_hit(1)){ CreateCycleMinLevel();   return true; }
		if(numeric_row_hit(2)){ CreateCycleMaxLevel();   return true; }
		if(numeric_row_hit(3)){ CreateCycleMaxPlayers(); return true; }
		if(numeric_row_hit(4)){ CreateCycleMaxTeams();   return true; }
		if(mouse_y >= 89 && mouse_y < 89 + 265 &&
		   mouse_x >= 407 && mouse_x < 407 + 214){
			const GameCreateState & gcs = state_data_.game_create;
			int row = ((mouse_y - 89) / 14) + gcs.map_scrolled;
			if(mouse_x >= 407 + 214 - 16){
				CreateDownloadMap(row);
			}else{
				CreateSelectMap(row);
			}
			return true;
		}
		if(mouse_x >= 410 && mouse_x < 410 + 210 &&
		   mouse_y >= 375 && mouse_y < 375 + 14){
			create_active_field_ = 1;
			state_data_.game_create.name_active = true;
			state_data_.game_create.password_active = false;
			return true;
		}
		if(mouse_x >= 410 && mouse_x < 410 + 210 &&
		   mouse_y >= 405 && mouse_y < 405 + 14){
			create_active_field_ = 2;
			state_data_.game_create.name_active = false;
			state_data_.game_create.password_active = true;
			return true;
		}
	}

	// Game-tech panel hit-tests.
	if(state_data_.active_panel == LobbyActivePanel::GameTech){
		const auto & gs = state_data_.game_tech;
		const int local_box_x = 452;
		const int local_box_w = 13;
		const int local_box_h = 13;
		const int row_y0      = 125;
		const int row_dy      = 13;
		const int label_x     = 467;
		const int label_y_off = 2;
		const int label_h     = 8;
		for(size_t r = 0; r < gs.items.size(); r++){
			int box_y = row_y0 + (int)r * row_dy;
			if(mouse_x >= local_box_x && mouse_x < local_box_x + local_box_w &&
			   mouse_y >= box_y       && mouse_y < box_y + local_box_h){
				GameTechToggle((int)r);
				return true;
			}
			int lab_y = box_y + label_y_off;
			int lab_w = (int)gs.items[r].label.size() * 6;
			if(lab_w > 0 &&
			   mouse_x >= label_x && mouse_x < label_x + lab_w &&
			   mouse_y >= lab_y   && mouse_y < lab_y + label_h){
				GameTechSelect((int)r);
				return true;
			}
		}
	}

	Context ctx{
		world_.resources,
		/*logical_w=*/logical_w,
		/*logical_h=*/logical_h,
		/*scale=*/scale,
		/*version=*/world_.GetVersion(),
	};
	ctx.mouse_x = mouse_x;
	ctx.mouse_y = mouse_y;
	ctx.state   = &state_;
	LobbyHandlers handlers = BuildLobbyHandlers(this);
	Node tree = BuildLobby(ctx, handlers, state_data_);
	Layout(tree, ctx);
	DispatchClicks(tree, ctx);
	return true;
}

bool LobbyRuntime::DispatchKeyDown(int sdl_scancode){
	if(ChatActive()){
		switch(sdl_scancode){
			case SDL_SCANCODE_BACKSPACE: ChatBackspace(); return true;
			case SDL_SCANCODE_RETURN:    ChatSubmit();    return true;
			default: break;
		}
	}
	if(CreateInputActive()){
		switch(sdl_scancode){
			case SDL_SCANCODE_BACKSPACE: CreateBackspace(); return true;
			case SDL_SCANCODE_RETURN:    CreateSubmit();    return true;
			default: break;
		}
	}
	if(!ChatActive() && !CreateInputActive()){
		switch(sdl_scancode){
			case SDL_SCANCODE_LEFT:
			case SDL_SCANCODE_UP:    NavPrev(); return true;
			case SDL_SCANCODE_RIGHT:
			case SDL_SCANCODE_DOWN:  NavNext(); return true;
			default: break;
		}
	}
	return false;
}

bool LobbyRuntime::DispatchTextInput(char ascii){
	if(ChatActive()){       ChatAppendChar(ascii);   return true; }
	if(CreateInputActive()){ CreateAppendChar(ascii); return true; }
	return false;
}

void LobbyRuntime::Tick(){
	using LAP = LobbyActivePanel;
	LobbyState & ls = state_data_;

	if(world_.lobby.state == Lobby::DISCONNECTED){
		world_.Disconnect();
		sctx_.GoToState(GameState::LOBBYCONNECT);
		return;
	}

	if(ls.active_panel == LAP::GameCreate){
		int us = sctx_.mapDownloader.mapUploadState.load(std::memory_order_acquire);
		if(us == 2){
			sctx_.mapDownloader.mapUploadState.store(0, std::memory_order_relaxed);
			const char * pw = sctx_.mapDownloader.pendingCreate.password.empty() ? nullptr : sctx_.mapDownloader.pendingCreate.password.c_str();
			world_.lobby.CreateGame(
				sctx_.mapDownloader.pendingCreate.gamename.c_str(),
				sctx_.mapDownloader.pendingCreate.mapname.c_str(),
				sctx_.mapDownloader.pendingCreate.maphash,
				pw,
				sctx_.mapDownloader.pendingCreate.securitylevel,
				sctx_.mapDownloader.pendingCreate.minlevel,
				sctx_.mapDownloader.pendingCreate.maxlevel,
				sctx_.mapDownloader.pendingCreate.maxplayers,
				sctx_.mapDownloader.pendingCreate.maxteams);
		}else if(us == 3){
			sctx_.mapDownloader.mapUploadState.store(0, std::memory_order_relaxed);
			game_.creategameclicked = false;
			if(game_.IsV2ProgressModalActive()) game_.PopV2Modal();
			game_.ShowV2Message("Could not upload map");
		}
		if(world_.lobby.creategamestatus == 1){
			world_.lobby.creategamestatus = 0;
			LobbyGame * lobbygame = world_.lobby.GetGameById(world_.lobby.createdgameid);
			if(lobbygame){
				Serializer data;
				lobbygame->Serialize(Serializer::WRITE, data);
				world_.gameinfo.Serialize(Serializer::READ, data);
				game_.JoinGame(*lobbygame, lobbygame->password);
				sctx_.mapDownloader.LoadMapData(sctx_.mapDownloader.FindMap(lobbygame->mapname, &lobbygame->maphash).c_str());
				game_.currentlobbygameid = lobbygame->id;
			}
		}else if(world_.lobby.creategamestatus != 100 && world_.lobby.creategamestatus != 0){
			world_.lobby.creategamestatus = 0;
			game_.creategameclicked = false;
			if(game_.IsV2ProgressModalActive()) game_.PopV2Modal();
			game_.ShowV2Message("Could not create game");
		}
	}

	if(ls.active_panel == LAP::GameSelect || ls.active_panel == LAP::GameCreate){
		if(game_.joininggame){
			if(world_.state == World::CONNECTED){
				game_.joininggame = false;
			}
			if(world_.state == World::IDLE){
				game_.joininggame = false;
				if(game_.IsV2ProgressModalActive()) game_.PopV2Modal();
				game_.ShowV2Message("Unable to join game");
			}
		}
		if(game_.IsV2ProgressModalActive()){
			std::string text = (sctx_.mapDownloader.mapUploadState.load(std::memory_order_relaxed) == 1)
				? "Uploading map" : "Creating game";
			int dots = (world_.tickcount / 4) % 6;
			if(dots > 3) dots = 6 - dots;
			for(int i = 0; i < dots; i++) text += ".";
			game_.SetV2ProgressText(text);
		}
		if(game_.IsV2ProgressModalActive() && world_.lobby.creategamestatus != 100 &&
		   sctx_.mapDownloader.mapUploadState.load(std::memory_order_relaxed) == 0 &&
		   (world_.state == World::CONNECTED || world_.state == World::IDLE)){
			game_.PopV2Modal();
			game_.creategameclicked = false;
		}
		if(world_.state == World::CONNECTED){
			Peer * peer = world_.peerlist[world_.localpeerid];
			if(peer){
				sctx_.mapDownloader.mapexistchecked = false;
				sctx_.mapDownloader.mapjoingeneration.fetch_add(1, std::memory_order_relaxed);
				sctx_.mapDownloader.mapjoinstate.store(0, std::memory_order_relaxed);
				if(sctx_.mapDownloader.mapjointhread.joinable()) sctx_.mapDownloader.mapjointhread.detach();
				world_.SetTech(Config::GetInstance().defaulttechchoices[Config::GetInstance().defaultagency]);
				ShowGameJoin();
				LobbyGame * lobbygame = world_.lobby.GetGameById(game_.currentlobbygameid);
				if(lobbygame){
					char temp[256];
					sctx_.ambienceMixer.GetGameChannelName(*lobbygame, temp);
					std::strcpy(world_.lobby.lastchannel, world_.lobby.channel);
					world_.lobby.JoinChannel(temp);
					ls.map_name = std::string(lobbygame->mapname).substr(0, 25);
				}
			}
		}
	}

	sctx_.mapDownloader.ProcessMapDownload();

	if(world_.state != World::CONNECTED && !game_.IsV2ModalActive()){
		if(ls.active_panel == LAP::GameJoin || ls.active_panel == LAP::GameTech){
			Game & g = game_;
			game_.ShowV2Message("Disconnected from game", [&g](){ g.GoBack(); });
		}
	}
}

void LobbyRuntime::NavPrev(){
	int & c = state_data_.nav_cursor;
	if(c < 0){
		c = kLobbyNavCount - 1;
	}else{
		c = (c + kLobbyNavCount - 1) % kLobbyNavCount;
	}
}

void LobbyRuntime::NavNext(){
	int & c = state_data_.nav_cursor;
	if(c < 0){
		c = 0;
	}else{
		c = (c + 1) % kLobbyNavCount;
	}
}

void LobbyRuntime::HandleBack(){
	using LAP = LobbyActivePanel;
	LobbyState & ls = state_data_;
	if(ls.active_panel == LAP::GameJoin || ls.active_panel == LAP::GameTech){
		game_.LeaveJoinedGame();
		ls.map_name.clear();
		ShowGameSelect();
		return;
	}
	if(ls.active_panel == LAP::GameCreate){
		world_.lobby.gamesprocessed = false;
		ShowGameSelect();
		return;
	}
	sctx_.GoToState(GameState::MAINMENU);
}

void LobbyRuntime::ShowGameSelect(){
	state_data_.game_select.selected_index = -1;
	state_data_.game_select.scrolled = 0;
	world_.lobby.gamesprocessed = false;
	state_data_.active_panel = LobbyActivePanel::GameSelect;
}

void LobbyRuntime::ShowGameCreate(){
	GameCreateState & gcs = state_data_.game_create;
	gcs = GameCreateState{};
	gcs.game_name = Config::GetInstance().defaultgamename;
	gcs.name_active     = true;
	gcs.password_active = false;
	create_active_field_ = 1;

	std::vector<std::string> maps;
	CDResDir();
	auto local = sctx_.mapDownloader.ListFiles((GetResDir() + "level").c_str());
	maps.insert(maps.end(), local.begin(), local.end());
	CDDataDir();
	auto downloads = sctx_.mapDownloader.ListFiles((GetDataDir() + "level/download").c_str());
	for(auto & d : downloads){
		if(std::find(maps.begin(), maps.end(), d) == maps.end()) maps.push_back(d);
	}
	std::sort(maps.begin(), maps.end());
	sctx_.mapDownloader.servermaps.clear();
	auto serverlist = FetchServerMapList(Config::GetInstance().mapapiurl);
	for(auto & entry : serverlist){
		if(std::find(maps.begin(), maps.end(), entry.first) == maps.end()){
			std::string label = "[DL] " + entry.first;
			maps.push_back(label);
			gcs.server_maps.insert(label);
			sctx_.mapDownloader.servermaps[label] = entry.second;
		}
	}
	gcs.map_items = std::move(maps);
	sctx_.mapDownloader.selectedmap = -1;
	game_.creategameclicked = false;

	state_data_.active_panel = LobbyActivePanel::GameCreate;
}

void LobbyRuntime::CreateAppendChar(char c){
	GameCreateState & gcs = state_data_.game_create;
	if(c < 0x20 || c > 0x7F) return;
	if(create_active_field_ == 1){
		if(gcs.game_name.size() >= 35) return;
		gcs.game_name.push_back(c);
	}else if(create_active_field_ == 2){
		if(gcs.password.size() >= 20) return;
		gcs.password.push_back(c);
	}
}

void LobbyRuntime::CreateBackspace(){
	GameCreateState & gcs = state_data_.game_create;
	if(create_active_field_ == 1 && !gcs.game_name.empty()){
		gcs.game_name.pop_back();
	}else if(create_active_field_ == 2 && !gcs.password.empty()){
		gcs.password.pop_back();
	}
}

void LobbyRuntime::CreateSubmit(){
	CreateGame();
}

void LobbyRuntime::CreateCycleSecurity(){
	GameCreateState & gcs = state_data_.game_create;
	if(gcs.security_text == "Off")         gcs.security_text = "Low";
	else if(gcs.security_text == "Low")    gcs.security_text = "Medium";
	else if(gcs.security_text == "Medium") gcs.security_text = "High";
	else                                   gcs.security_text = "Off";
}

void LobbyRuntime::CreateCycleMinLevel()  { CycleNumeric(state_data_.game_create.min_level_text,   0,  99); }
void LobbyRuntime::CreateCycleMaxLevel()  { CycleNumeric(state_data_.game_create.max_level_text,   0,  99); }
void LobbyRuntime::CreateCycleMaxPlayers(){ CycleNumeric(state_data_.game_create.max_players_text, 1,  24); }
void LobbyRuntime::CreateCycleMaxTeams()  { CycleNumeric(state_data_.game_create.max_teams_text,   1,  16); }

void LobbyRuntime::CreateSelectMap(int row){
	GameCreateState & gcs = state_data_.game_create;
	if(row < 0 || row >= (int)gcs.map_items.size()){
		gcs.selected_map = -1;
		sctx_.mapDownloader.selectedmap = -1;
		return;
	}
	gcs.selected_map = row;
	sctx_.mapDownloader.selectedmap = row;
}

void LobbyRuntime::CreateDownloadMap(int row){
	GameCreateState & gcs = state_data_.game_create;
	if(!sctx_.mapDownloader.dlitemname.empty()) return;
	if(row < 0 || row >= (int)gcs.map_items.size()) return;
	const std::string & item = gcs.map_items[row];
	auto dlit = sctx_.mapDownloader.servermaps.find(item);
	if(dlit == sctx_.mapDownloader.servermaps.end()) return;
	const std::string & hex = dlit->second;
	if(hex.size() != 40) return;
	unsigned char sha1bytes[20] = {};
	auto hv = [](char c) -> int {
		if(c >= '0' && c <= '9') return c - '0';
		if(c >= 'a' && c <= 'f') return c - 'a' + 10;
		if(c >= 'A' && c <= 'F') return c - 'A' + 10;
		return -1;
	};
	for(int j = 0; j < 20; j++){
		int hi = hv(hex[2*j]), lo = hv(hex[2*j+1]);
		if(hi < 0 || lo < 0) return;
		sha1bytes[j] = (unsigned char)((hi << 4) | lo);
	}
	std::string dlname = item.substr(5);
	sctx_.mapDownloader.dlitemname = item;
	sctx_.mapDownloader.dlresult.store(0);
	sctx_.mapDownloader.dlprogress.store(0);
	gcs.download_item = dlname;
	gcs.download_progress = 0;
	if(sctx_.mapDownloader.dlthread.joinable()) sctx_.mapDownloader.dlthread.join();
	std::string apiURL = Config::GetInstance().mapapiurl;
	std::atomic<int> * progressPtr = &sctx_.mapDownloader.dlprogress;
	std::atomic<int> * resultPtr   = &sctx_.mapDownloader.dlresult;
	sctx_.mapDownloader.dlthread = std::thread([dlname, sha1bytes, apiURL, progressPtr, resultPtr]() mutable {
		std::string res = FetchMapFromServer(dlname.c_str(), sha1bytes, apiURL.c_str(), progressPtr);
		resultPtr->store(res.empty() ? -1 : 1);
	});
}

void LobbyRuntime::CreateGame(){
	GameCreateState & gcs = state_data_.game_create;
	if(game_.creategameclicked) return;

	if(gcs.game_name.empty()){
		game_.ShowV2Message("No game name");
		return;
	}
	if(gcs.selected_map < 0 || gcs.selected_map >= (int)gcs.map_items.size()){
		game_.ShowV2Message("No map selected");
		return;
	}
	const std::string & mapname = gcs.map_items[gcs.selected_map];
	if(gcs.server_maps.count(mapname) > 0){
		game_.ShowV2Message("Download the map first");
		return;
	}

	Uint8 securitylevel = LobbyGame::SECNONE;
	if(gcs.security_text == "Low")         securitylevel = LobbyGame::SECLOW;
	else if(gcs.security_text == "Medium") securitylevel = LobbyGame::SECMEDIUM;
	else if(gcs.security_text == "High")   securitylevel = LobbyGame::SECHIGH;

	Uint8 minlevel   = (Uint8)std::atoi(gcs.min_level_text.c_str());
	Uint8 maxlevel   = (Uint8)std::atoi(gcs.max_level_text.c_str());
	Uint8 maxplayers = (Uint8)std::atoi(gcs.max_players_text.c_str());
	if(maxplayers == 0) maxplayers = 1;
	Uint8 maxteams   = (Uint8)std::atoi(gcs.max_teams_text.c_str());
	if(maxteams == 0) maxteams = 1;

	unsigned char maphash[20];
	sctx_.mapDownloader.CalculateMapHash(sctx_.mapDownloader.FindMap(mapname.c_str()).c_str(), &maphash);
	sctx_.mapDownloader.pendingCreate.gamename = gcs.game_name;
	sctx_.mapDownloader.pendingCreate.mapname  = mapname;
	sctx_.mapDownloader.pendingCreate.password = gcs.password;
	std::memcpy(sctx_.mapDownloader.pendingCreate.maphash, maphash, 20);
	sctx_.mapDownloader.pendingCreate.securitylevel = securitylevel;
	sctx_.mapDownloader.pendingCreate.minlevel      = minlevel;
	sctx_.mapDownloader.pendingCreate.maxlevel      = maxlevel;
	sctx_.mapDownloader.pendingCreate.maxplayers    = maxplayers;
	sctx_.mapDownloader.pendingCreate.maxteams      = maxteams;
	if(sctx_.mapDownloader.mapUploadThread.joinable()) sctx_.mapDownloader.mapUploadThread.detach();
	uint32_t gen = ++sctx_.mapDownloader.mapUploadGeneration;
	std::string mppath = sctx_.mapDownloader.FindMap(mapname.c_str());
	std::string dataDir = GetDataDir();
	bool isBundledMap = dataDir.empty() || mppath.substr(0, dataDir.size()) != dataDir;
	std::string apiURL = Config::GetInstance().mapapiurl;
	if(isBundledMap){
		sctx_.mapDownloader.mapUploadState.store(2, std::memory_order_release);
	}else{
		sctx_.mapDownloader.mapUploadState.store(1, std::memory_order_relaxed);
		std::string mapname_str = mapname;
		std::atomic<int> * uploadStatePtr = &sctx_.mapDownloader.mapUploadState;
		std::atomic<uint32_t> * uploadGenPtr = &sctx_.mapDownloader.mapUploadGeneration;
		sctx_.mapDownloader.mapUploadThread = std::thread([mapname_str, mppath, apiURL, gen, uploadStatePtr, uploadGenPtr](){
			bool ok = UploadMapToServer(mapname_str.c_str(), mppath.c_str(), apiURL.c_str());
			if(uploadGenPtr->load(std::memory_order_relaxed) != gen) return;
			uploadStatePtr->store(ok ? 2 : 3, std::memory_order_release);
		});
	}
	game_.creategameclicked = true;
	std::strncpy(Config::GetInstance().defaultgamename, gcs.game_name.c_str(),
	             sizeof(Config::GetInstance().defaultgamename) - 1);
	Config::GetInstance().defaultgamename[sizeof(Config::GetInstance().defaultgamename) - 1] = '\0';
	Config::GetInstance().Save();
	game_.ShowV2ProgressMessage("Uploading map...");
}

void LobbyRuntime::ShowGameJoin(){
	if(world_.choosingtech){
		world_.choosingtech = false;
		game_.ShowTeamOverlays(true);
	}
	state_data_.game_join = GameJoinState{};
	state_data_.active_panel = LobbyActivePanel::GameJoin;
}

void LobbyRuntime::GameJoinReady(){
	Peer * localpeer = world_.peerlist[world_.localpeerid];
	bool ishost = localpeer && localpeer->ishost;
	if(!ishost || world_.AllPeersDownloadedMap()){
		world_.SendReady();
	}
}

void LobbyRuntime::GameJoinChangeTeam(){
	world_.ChangeTeam();
}

void LobbyRuntime::ShowGameTech(){
	world_.choosingtech = true;
	game_.ShowTeamOverlays(false);
	world_.RequestPeerList();
	state_data_.game_tech = GameTechState{};
	state_data_.active_panel = LobbyActivePanel::GameTech;
}

void LobbyRuntime::GameTechToggle(int item_index){
	if(state_data_.active_panel != LobbyActivePanel::GameTech) return;
	auto & gs = state_data_.game_tech;
	if(item_index < 0 || item_index >= (int)gs.items.size()) return;
	if(!gs.items[item_index].local_usable) return;
	Team * team = world_.GetPeerTeam(world_.localpeerid);
	if(!team) return;
	int seen = 0;
	for(auto * buyableitem : world_.buyableitems){
		if(!buyableitem->techslots) continue;
		if(buyableitem->agencyspecific != -1 && buyableitem->agencyspecific != team->agency){
			continue;
		}
		if(seen == item_index){
			Peer * lp = world_.peerlist[world_.localpeerid];
			if(lp){
				world_.SetTech(lp->techchoices ^ buyableitem->techchoice);
				Config::GetInstance().defaulttechchoices[team->agency] =
					lp->techchoices ^ buyableitem->techchoice;
				Config::GetInstance().Save();
			}
			return;
		}
		seen++;
	}
}

void LobbyRuntime::GameTechSelect(int item_index){
	if(state_data_.active_panel != LobbyActivePanel::GameTech) return;
	auto & gs = state_data_.game_tech;
	if(item_index < 0 || item_index >= (int)gs.items.size()) return;
	Team * team = world_.GetPeerTeam(world_.localpeerid);
	if(!team) return;
	int seen = 0;
	for(auto * buyableitem : world_.buyableitems){
		if(!buyableitem->techslots) continue;
		if(buyableitem->agencyspecific != -1 && buyableitem->agencyspecific != team->agency){
			continue;
		}
		if(seen == item_index){
			gs.tech_name_text = std::string("-") + buyableitem->name + "-";
			gs.tech_name_x = (Sint16)(401 + (116 - ((gs.tech_name_text.length() * 8) / 2)));
			char desc[1024];
			std::strncpy(desc, buyableitem->description, sizeof(desc) - 1);
			desc[sizeof(desc) - 1] = '\0';
			int linenum = 0;
			char * descline = std::strtok(desc, "\n");
			while(descline && linenum < 8){
				gs.tech_desc_lines[linenum] = descline;
				linenum++;
				descline = std::strtok(nullptr, "\n");
			}
			for(int j = linenum; j < 8; j++){
				gs.tech_desc_lines[j].clear();
			}
			return;
		}
		seen++;
	}
}

void LobbyRuntime::SelectAgency(Uint8 agency){
	if(agency >= 5) return;
	if(state_data_.character.selected_agency == agency) return;
	state_data_.character.selected_agency = agency;
	Config::GetInstance().defaultagency = agency;
	Config::GetInstance().Save();
	if(world_.IsConnected()){
		world_.SetAgency(agency);
	}
}

void LobbyRuntime::GameSelectRow(int index){
	GameSelectState & gs = state_data_.game_select;
	if(index < 0 || index >= (int)gs.games.size()){
		gs.selected_index = -1;
		return;
	}
	gs.selected_index = index;
}

void LobbyRuntime::GameSelectJoin(){
	GameSelectState & gs = state_data_.game_select;
	if(gs.selected_index < 0 || gs.selected_index >= (int)gs.games.size()){
		game_.ShowV2Message("No game selected");
		return;
	}
	Uint32 gameid = gs.games[gs.selected_index].id;
	if(!gameid) return;
	LobbyGame * lobbygame = world_.lobby.GetGameById(gameid);
	if(!lobbygame) return;
	if(!world_.IsIdle()) return;
	User * user = world_.lobby.GetUserInfo(world_.lobby.accountid);
	if(user){
		if(lobbygame->minlevel > user->agency[Config::GetInstance().defaultagency].level){
			game_.ShowV2Message("Your player level is too low");
			return;
		}
		if(lobbygame->maxlevel < user->agency[Config::GetInstance().defaultagency].level){
			game_.ShowV2Message("Your player level is too high");
			return;
		}
	}
	game_.currentlobbygameid = lobbygame->id;
	if(std::strlen(lobbygame->password) > 0 && lobbygame->accountid != world_.lobby.accountid){
		Game & g = game_;
		Uint32 gid = lobbygame->id;
		game_.ShowV2PasswordModal([&g, gid](const std::string & password){
			LobbyGame * lg = g.GetWorld().lobby.GetGameById(gid);
			if(lg){
				char buf[64];
				std::strncpy(buf, password.c_str(), sizeof(buf) - 1);
				buf[sizeof(buf) - 1] = '\0';
				g.JoinGame(*lg, buf);
			}
		});
	}else{
		game_.JoinGame(*lobbygame);
	}
}

void LobbyRuntime::ChatAppendChar(char c){
	ChatPanelState & cs = state_data_.chat;
	if(c < 0x20 || c > 0x7F) return;
	if(cs.chat_input.size() >= 200) return;
	if(cs.chat_input.size() >= (size_t)(60 + cs.chat_input_scrolled)){
		cs.chat_input_scrolled++;
	}
	cs.chat_input.push_back(c);
}

void LobbyRuntime::ChatBackspace(){
	ChatPanelState & cs = state_data_.chat;
	if(cs.chat_input.empty()) return;
	cs.chat_input.pop_back();
	if(cs.chat_input_scrolled > 0) cs.chat_input_scrolled--;
}

void LobbyRuntime::ChatSubmit(){
	ChatPanelState & cs = state_data_.chat;
	if(cs.chat_input.empty()) return;
	world_.lobby.SendChat(world_.lobby.channel, cs.chat_input.c_str());
	cs.chat_input.clear();
	cs.chat_input_scrolled = 0;
}

}  // namespace v2
}  // namespace ui
