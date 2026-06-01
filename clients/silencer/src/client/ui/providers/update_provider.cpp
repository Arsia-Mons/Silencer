#include "client/ui/hooks/use_update.h"

#include "screen_context.h"
#include "updater.h"
#include "updaterstage2.h"

#include <cstdio>
#include <cstdlib>

namespace silencer {
namespace client_ui {

UpdateProviderValue MakeUpdateProvider(ScreenContext& ctx) {
	UpdateProviderValue value;
	value.updater = &ctx.updater;
	return value;
}

namespace update_provider_detail {

Updater * UpdaterFor(const UpdateProviderValue& provider) {
	return provider.updater;
}

UpdateStatus ConvertState(Updater::State state) {
	switch(state){
		case Updater::PROMPTING: return UpdateStatus::Prompting;
		case Updater::DOWNLOADING: return UpdateStatus::Downloading;
		case Updater::VERIFYING: return UpdateStatus::Verifying;
		case Updater::STAGING: return UpdateStatus::Staging;
		case Updater::FAILED: return UpdateStatus::Failed;
		case Updater::DONE: return UpdateStatus::Done;
		case Updater::IDLE:
		default:
			return UpdateStatus::Idle;
	}
}

std::string StatusText(Updater& updater) {
	switch(updater.GetState()){
		case Updater::PROMPTING:
			return "An update is required to play online.";
		case Updater::DOWNLOADING:{
			char buf[32];
			snprintf(buf, sizeof(buf), "%d%%", int(updater.GetProgress() * 100));
			return buf;
		}
		case Updater::VERIFYING:
			return "Verifying...";
		case Updater::STAGING:
			return "Restarting...";
		case Updater::FAILED:
			return updater.GetErrorMessage();
		case Updater::IDLE:
		case Updater::DONE:
		default:
			return "";
	}
}

std::string ProgressText(Updater& updater) {
	if(updater.GetState() != Updater::DOWNLOADING) return "";
	int width = int(updater.GetProgress() * 20.0f);
	std::string bar = "[";
	for(int i = 0; i < 20; i++) bar += (i < width) ? "=" : " ";
	bar += "]";
	return bar;
}

std::string Stage2ZipPath() {
#ifdef _WIN32
	return std::string(getenv("TEMP") ? getenv("TEMP") : ".") + "\\silencer-update.zip";
#else
	return "/tmp/silencer-update.zip";
#endif
}

bool OpenExternalUrl(const std::string& url) {
	if(url.empty()) return false;
#ifdef _WIN32
	std::string cmd = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
	std::string cmd = "open '" + url + "'";
#else
	std::string cmd = "xdg-open '" + url + "' &";
#endif
	(void)system(cmd.c_str());
	return true;
}

}  // namespace update_provider_detail

UpdateModel::UpdateModel(const UpdateProviderValue& provider)
	: provider_(provider) {}

UpdateSnapshot UpdateModel::snapshot() const {
	Updater * updater = update_provider_detail::UpdaterFor(provider_);
	if(!updater) return UpdateSnapshot{};
	const UpdateStatus status = update_provider_detail::ConvertState(updater->GetState());
	UpdateSnapshot view;
	view.status = status;
	view.status_text = update_provider_detail::StatusText(*updater);
	view.progress_text = update_provider_detail::ProgressText(*updater);
	view.can_update = status == UpdateStatus::Prompting;
	view.can_cancel = status == UpdateStatus::Prompting ||
	                  status == UpdateStatus::Downloading ||
	                  status == UpdateStatus::Failed;
	view.can_retry = status == UpdateStatus::Failed && updater->GetRetryCount() < 3;
	view.can_download = status == UpdateStatus::Failed && updater->GetRetryCount() >= 3;
	return view;
}

void UpdateModel::consent() const {
	Updater * updater = update_provider_detail::UpdaterFor(provider_);
	if(updater && updater->GetState() == Updater::PROMPTING){
		updater->Consent();
	}
}

bool UpdateModel::cancel() const {
	Updater * updater = update_provider_detail::UpdaterFor(provider_);
	if(!updater) return false;
	Updater::State state = updater->GetState();
	if(state != Updater::PROMPTING &&
	   state != Updater::DOWNLOADING &&
	   state != Updater::FAILED){
		return false;
	}
	if(state == Updater::DOWNLOADING){
		updater->Cancel();
	}
	return true;
}

void UpdateModel::retry() const {
	Updater * updater = update_provider_detail::UpdaterFor(provider_);
	if(updater && updater->GetState() == Updater::FAILED &&
	   updater->GetRetryCount() < 3){
		updater->Retry();
	}
}

bool UpdateModel::open_download() const {
	Updater * updater = update_provider_detail::UpdaterFor(provider_);
	if(!updater || updater->GetState() != Updater::FAILED ||
	   updater->GetRetryCount() < 3){
		return false;
	}
	return update_provider_detail::OpenExternalUrl(updater->GetDownloadURL());
}

UpdateStage2Result UpdateModel::launch_stage2_if_ready() const {
	Updater * updater = update_provider_detail::UpdaterFor(provider_);
	if(!updater || updater->GetState() != Updater::STAGING){
		return UpdateStage2Result::NotReady;
	}
	const std::string zippath = update_provider_detail::Stage2ZipPath();
	fprintf(stderr, "[updater] UpdateScreen invoking UpdaterStage2::Launch with zip=%s\n",
	        zippath.c_str());
	if(UpdaterStage2::Launch(zippath)){
		updater->MarkStage2Spawned();
		return UpdateStage2Result::Spawned;
	}
	fprintf(stderr, "[updater] UpdaterStage2::Launch failed; returning to main menu\n");
	return UpdateStage2Result::Failed;
}

UpdateModel use_update(const UpdateProviderValue& provider) {
	return UpdateModel(provider);
}

}  // namespace client_ui
}  // namespace silencer
