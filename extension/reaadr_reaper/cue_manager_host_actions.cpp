// Load standard headers before REAPER/WDL declarations: SWELL defines
// min/max macros on non-Windows platforms.
#include <array>
#include <cstring>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "cue_manager_host_actions.hpp"
#include "app/cue_import_request.hpp"
#include "app/cue_import_workflow.hpp"
#include "app/session_export_service.hpp"
#include "legacy_host.hpp"
#include "native_runtime.hpp"
#include "project_state.hpp"
#include "cue_manager_session.hpp"
#include <reaper_plugin.h>
#include "reaper_api.hpp"

namespace reaadr::reaper {
namespace {
std::string export_path(const char* title) {
  if (!GetUserInputs) return {};
  std::array<char,4096> path{};
  if (!GetUserInputs(title,1,"Output CSV path",path.data(),path.size())) return {};
  std::string out(path.data());
  const auto first=out.find_first_not_of(" \t\r\n"), last=out.find_last_not_of(" \t\r\n");
  if(first==std::string::npos)return {};
  out=out.substr(first,last-first+1);
  const auto slash=out.find_last_of("/\\"), dot=out.find_last_of('.');
  if(dot==std::string::npos||(slash!=std::string::npos&&dot<slash))out+=".csv";
  return out;
}
void export_action(const std::string& action) {
  const char* title=action=="export_cue_sheet"?"ReaADR: Export Cue Sheet":
    action=="export_timing_report"?"ReaADR: Export Timing Report":"ReaADR: Export Session Metadata";
  const auto path=export_path(title); if(path.empty())return;
  const auto loaded=cue_manager_session_host().load_session();
  if(!loaded){ShowMessageBox(core::session_load_error_message(loaded).c_str(),"ReaADR Export",0);return;}
  const auto kind=action=="export_cue_sheet"?SessionExportKind::cue_sheet:
    action=="export_timing_report"?SessionExportKind::timing_report:SessionExportKind::session_metadata;
  const auto formatted=format_session_export(loaded.model,kind);
  if(!formatted){ShowMessageBox(formatted.error.c_str(),"ReaADR Export",0);return;}
  std::string error;
  if(!write_session_export(path,formatted.content,&error)){ShowMessageBox(error.c_str(),"ReaADR Export",0);return;}
  ShowMessageBox(("Exported to:\n"+path).c_str(),"ReaADR Export",0);
}
void clear_character_cues_action() {
  if(!GetUserInputs)return;
  std::array<char,4096> input{};
  if(!GetUserInputs("ReaADR: Clear Character Cues",1,"Characters (semicolon-separated):",input.data(),input.size()))return;
  std::vector<std::string> characters;std::stringstream stream(input.data());std::string value;
  while(std::getline(stream,value,';')){
    const auto first=value.find_first_not_of(" \t\r\n"),last=value.find_last_not_of(" \t\r\n");
    if(first!=std::string::npos)characters.push_back(value.substr(first,last-first+1));
  }
  std::string error;const auto result=cue_manager_session_host().clear_characters(characters,error);
  if(!result){if(!error.empty())ShowMessageBox(error.c_str(),"ReaADR Clear Character Cues",0);return;}
  ShowMessageBox(("Removed "+std::to_string(result.cues_removed)+" cue(s).").c_str(),"ReaADR Clear Character Cues",0);
}
void manager_import(const std::string& serialized_mapping,bool preview_only,
                    const std::string& mode,const std::string& serialized_characters) {
  CueImportWorkflowRequest request;request.serialized_mapping=serialized_mapping;
  request.preview_only=preview_only;request.mode=mode;request.serialized_characters=serialized_characters;
  if(serialized_mapping.empty()){
    std::array<char,4096> saved{};
    if(GetProjExtState&&GetProjExtState(nullptr,"ReaADRTools","import_mapping_last",saved.data(),saved.size())>0)
      request.persisted_mapping=saved.data();
  }
  CueImportHostUi ui;
  ui.choose_source=[preview_only](std::string& path){
    if(!GetUserInputs)return false;std::array<char,4096> value{};
    if(!GetUserInputs(preview_only?"ReaADR: Preview Cue Sheet Import":"ReaADR: Import Cue Sheet",1,"Cue sheet path",value.data(),value.size()))return false;
    path=value.data();return true;
  };
  ui.prompt=[](const std::string& title,const std::string& caption,std::string& value){
    if(!GetUserInputs)return false;std::array<char,2048> input{};
    if(!value.empty())std::strncpy(input.data(),value.c_str(),input.size()-1);
    if(!GetUserInputs(title.c_str(),1,caption.c_str(),input.data(),input.size()))return false;
    value=input.data();return true;
  };
  ui.message=[](const std::string& title,const std::string& message){if(ShowMessageBox)ShowMessageBox(message.c_str(),title.c_str(),0);};
  CueImportWorkflowCallbacks callbacks;
  callbacks.preview=[](const std::string& content,const std::string& source,const std::optional<core::ColumnMapping>& mapping){
    return cue_manager_session_host().preview_import_content(content,source,mapping);
  };
  callbacks.import=[](const std::string& content,const std::string& source,const std::optional<core::ColumnMapping>& mapping,
                       const std::string& import_mode,const std::vector<std::string>& characters){
    return cue_manager_session_host().import_content(content,source,mapping,import_mode,characters);
  };
  run_cue_import_workflow(request,ui,callbacks);
}
} // namespace

void run_persistent_native_import_action() {
  CueImportWorkflowRequest request;
  std::array<char,4096> saved{};
  if(GetProjExtState&&GetProjExtState(nullptr,"ReaADRTools","import_mapping_last",saved.data(),saved.size())>0)
    request.persisted_mapping=saved.data();
  CueImportHostUi ui;
  ui.choose_source=[](std::string& source){
    if(!GetUserFileNameForRead)return false;std::array<char,4096> path{};
    if(!GetUserFileNameForRead(path.data(),"ReaADR: Import Cue Sheet","csv;tsv;tab;txt;xlsx"))return false;
    source=path.data();return true;
  };
  ui.prompt=[](const std::string& title,const std::string& caption,std::string& value){
    if(!GetUserInputs)return false;std::array<char,2048> input{};
    if(!value.empty())std::strncpy(input.data(),value.c_str(),input.size()-1);
    if(!GetUserInputs(title.c_str(),1,caption.c_str(),input.data(),input.size()))return false;
    value=input.data();return true;
  };
  ui.message=[](const std::string& title,const std::string& message){if(ShowMessageBox)ShowMessageBox(message.c_str(),title.c_str(),0);};
  CueImportWorkflowCallbacks callbacks;
  callbacks.preview=[](const std::string& content,const std::string& source,const std::optional<core::ColumnMapping>& mapping){
    return cue_manager_session_host().preview_import_content(content,source,mapping);
  };
  callbacks.import=[](const std::string& content,const std::string& source,const std::optional<core::ColumnMapping>& mapping,
                       const std::string& mode,const std::vector<std::string>& characters){
    return cue_manager_session_host().import_content(content,source,mapping,mode,characters);
  };
  if(!cue_manager_session_host().has_session()){
    CueManagerSessionConfig config;
    config.project_state_api={GetProjExtState,SetProjExtState};config.global_state_api={GetExtState,SetExtState};
    config.cleanup_api=legacy_host::cue_cleanup_api();
    config.callbacks.trigger_import=[](const std::string& mapping,bool preview,const std::string& mode,const std::string& chars){
      manager_import(mapping,preview,mode,chars);
    };
    std::string error;
    if(!open_native_cue_manager(std::move(config),error)){if(!error.empty())ShowMessageBox(error.c_str(),"ReaADR Import",0);return;}
  }
  run_cue_import_workflow(request,ui,callbacks);
}

void run_persistent_native_cue_manager_action() {
  CueManagerSessionConfig config;
  config.project_state_api={GetProjExtState,SetProjExtState};
  config.global_state_api={GetExtState,SetExtState};
  config.cleanup_api=legacy_host::cue_cleanup_api();
  config.callbacks.trigger_import=[](const std::string& mapping,bool preview,const std::string& mode,const std::string& chars){
    manager_import(mapping,preview,mode,chars);
  };
  config.callbacks.trigger_action=[](const std::string& action){
    if(action=="sync_regions"){
      std::string error;
      if(!cue_manager_session_host().sync_regions(error)&&!error.empty())ShowMessageBox(error.c_str(),"ReaADR Cue Manager",0);
    }else if(action=="clear_character_cues")clear_character_cues_action();
    else if(action=="export_cue_sheet"||action=="export_timing_report"||action=="export_session_metadata")export_action(action);
  };
  std::string error;
  if(!open_native_cue_manager(std::move(config),error)&&!error.empty())ShowMessageBox(error.c_str(),"ReaADR Cue Manager",0);
}
} // namespace reaadr::reaper
