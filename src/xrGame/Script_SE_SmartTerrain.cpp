#include "stdafx.h"
#include "Script_SE_SmartTerrain.h"
#include "Script_GulagGenerator.h"

CScriptIniFile ini_file_locations = CScriptIniFile("misc\\smart_terrain_masks.ltx");

namespace Cordis
{
namespace Scripts
{
namespace XR_LOGIC
{
void activate_by_section(CScriptGameObject* const p_client_object, CScriptIniFile* const p_ini,
    const xr_string& section_name, const xr_string& gulag_name, const bool is_loading)
{
    xr_string _section_name; // @ if argument is empty string for section_name
    if (!p_ini)
    {
        R_ASSERT2(false, "object is null!");
        return;
    }

    if (!p_client_object)
    {
        R_ASSERT2(false, "object is null!");
        return;
    }

    const std::uint16_t npc_id = p_client_object->ID();

    if (!is_loading)
    {
        DataBase::Storage::getInstance().setStorageActivationTime(npc_id, Globals::get_time_global());
        DataBase::Storage::getInstance().setStorageActivationGameTime(npc_id, Globals::Game::get_game_time());
    }

    if (section_name == "nil")
    {
        DataBase::Storage::getInstance().setStorageOverrides(npc_id, DataBase::Data_Overrides());
        reset_generic_schemes_on_scheme_switch(p_client_object, "nil", "nil"); // имеет ли смысл?
        DataBase::Storage::getInstance().setStorageActiveSectionName(npc_id, "");
        DataBase::Storage::getInstance().setStorageActiveSchemeName(npc_id, "");
        return;
    }

    if (section_name.empty())
    {
        CSE_ALifeDynamicObject* p_server_smart = XR_GULAG::get_npc_smart(p_client_object);
        if (!p_server_smart)
        {
            R_ASSERT2(false, "section is NIL and NPC not in smart_terrain!");
        }

        const xr_map<std::uint32_t, JobDataSmartTerrain*>& job =
            p_server_smart->cast_script_se_smartterrain()->getJobData();

        if (job.at(p_client_object->ID()))
        {
            _section_name = job.at(p_client_object->ID())->m_job_id.first;
        }
    }

    if (section_name.empty())
    {
        if (!p_ini->section_exist(_section_name.c_str()))
        {
            R_ASSERT2(false, "doesnt exist!");
        }
    }
    else
    {
        if (!p_ini->section_exist(section_name.c_str()))
        {
            R_ASSERT2(false, "doesnt exist!");
        }
    }

    xr_string _scheme_name = section_name.empty() ? Globals::Utils::get_scheme_by_section(_section_name) :
                                                    Globals::Utils::get_scheme_by_section(xr_string(section_name));
    if (_scheme_name.empty())
    {
        R_ASSERT2(false, "can't detect scheme name");
    }

    DataBase::Storage::getInstance().setStorageOverrides(
        npc_id, cfg_get_overrides(p_ini, section_name.empty() ? _section_name : section_name, p_client_object));

    reset_generic_schemes_on_scheme_switch(
        p_client_object, _scheme_name, section_name.empty() ? _section_name : section_name);

    if (!Script_GlobalHelper::getInstance().getSchemesSetSchemeCallbacks()[_scheme_name])
    {
        R_ASSERT2(false, "doesn't exist function for _scheme_name! Check Script_GlobalHelper::ctor()");
        return;
    }

    Script_GlobalHelper::getInstance().getSchemesSetSchemeCallbacks()[_scheme_name](
        p_client_object, p_ini, _scheme_name, section_name.empty() ? _section_name : section_name, gulag_name);

    DataBase::Storage::getInstance().setStorageActiveSectionName(
        npc_id, section_name.empty() ? _section_name : section_name);
    DataBase::Storage::getInstance().setStorageActiveSchemeName(npc_id, _scheme_name);

    if (DataBase::Storage::getInstance().getStorage().at(npc_id).getSchemeType() == Globals::kSTypeStalker)
    {
        Globals::Utils::send_to_nearest_accessible_vertex(p_client_object, p_client_object->level_vertex_id());
        // LorD: доделать когда будет activate_scheme
    }
    else
    {
        for (Script_ISchemeEntity* it :
            DataBase::Storage::getInstance().getStorage().at(npc_id).getSchemes().at(_scheme_name).getActions())
        {
            it->reset_scheme(is_loading, p_client_object);
        }
    }
}

// Lord: проверить на memory leak, особенно проверить факт будет ли повторно заходить в ветку else где выделяется память
// под p_actual_ini
CScriptIniFile* configure_schemes(CScriptGameObject* const p_client_object, CScriptIniFile* const p_ini,
    const xr_string& ini_filename, std::uint32_t stype, const xr_string& section_logic_name,
    const xr_string& gulag_name)
{
    const std::uint16_t npc_id = p_client_object->ID();
    const DataBase::Storage_Data& storage = DataBase::Storage::getInstance().getStorage().at(npc_id);

    if (!storage.getActiveSectionName().empty())
    {
        for (Script_ISchemeEntity* it : storage.getSchemes().at(storage.getActiveSchemeName()).getActions())
        {
            it->deactivate(p_client_object);
        }
    }

    xr_string actual_ini_filename;
    CScriptIniFile* p_actual_ini = nullptr;
    static bool is_allocated_for_storage =
        false; // если всё таки выделяли память, то в dtor() сторага будем сами удалять
    if (!p_ini->section_exist(section_logic_name.c_str()))
    {
        if (gulag_name.empty())
        {
            actual_ini_filename = ini_filename;
            p_actual_ini = p_ini;
        }
        else
        {
            R_ASSERT2(false, "YOu don't give a job to your entity!");
        }
    }
    else
    {
        xr_string filename = Globals::Utils::cfg_get_string(p_ini, section_logic_name, "cfg");
        if (!filename.empty())
        {
            actual_ini_filename = filename;
            p_actual_ini = new CScriptIniFile(filename.c_str());
            is_allocated_for_storage = true;
            if (!p_actual_ini->section_exist(section_logic_name.c_str()))
            {
                R_ASSERT2(false, "NOT FOUND!");
            }

            return configure_schemes(
                p_client_object, p_actual_ini, actual_ini_filename, stype, section_logic_name, gulag_name);
        }
        else
        {
            if (stype == Globals::kSTypeStalker || stype == Globals::kSTypeMobile)
            {
                Script_SE_SmartTerrain* p_server_smart =
                    XR_GULAG::get_npc_smart(p_client_object)->cast_script_se_smartterrain();
                if (p_server_smart)
                {
                    JobDataSmartTerrain* job = p_server_smart->getJobData()[npc_id];
                    if (job)
                    {
                        DataBase::Storage::getInstance().setStorageJobIniName(npc_id, job->m_ini_path_name);
                    }
                }
            }

            actual_ini_filename = ini_filename;
            p_actual_ini = p_ini;
            is_allocated_for_storage = false; // Lord: Needz here. check it pls
        }
    }

    disable_generic_schemes(p_client_object, stype);
    enable_generic_schemes(p_actual_ini, p_client_object, stype, section_logic_name);

    DataBase::Storage::getInstance().setStorageActiveSectionName(npc_id, "");
    DataBase::Storage::getInstance().setStorageActiveSchemeName(npc_id, "");
    DataBase::Storage::getInstance().setStorageGulagName(npc_id, gulag_name);
    DataBase::Storage::getInstance().setStorageSType(npc_id, stype);
    DataBase::Storage::getInstance().setStorageIniFile(npc_id, p_actual_ini, is_allocated_for_storage);
    DataBase::Storage::getInstance().setStorageInifilename(npc_id, actual_ini_filename);
    DataBase::Storage::getInstance().setStorageSectionLogicName(npc_id, section_logic_name);

    if (stype == Globals::kSTypeStalker)
    {
        // Lord: доделать когда будет trader_manager, spawner
    }

    return storage.getIni();
}
}
} // namespace Scripts
} // namespace Cordis

namespace Cordis
{
namespace Scripts
{
bool arrived_to_smart(CSE_ALifeMonsterAbstract* object, Script_SE_SmartTerrain* smart)
{
    if (!object)
    {
        R_ASSERT2(false, "can't use an empty object!");
        return false;
    }

    if (!smart)
    {
        R_ASSERT2(false, "can't use an empty object!");
        return false;
    }

    const CGameGraph::CVertex* object_vertex = nullptr;
    Fvector object_position;

    if (DataBase::Storage::getInstance().getStorage().find(object->ID) ==
        DataBase::Storage::getInstance().getStorage().end())
    {
        object_vertex = Globals::Game::get_game_graph()->vertex(object->m_tGraphID);
        object_position = object->position();
    }
    else
    {
        const DataBase::Storage_Data& storage = DataBase::Storage::getInstance().getStorage().at(object->ID);
        CScriptGameObject* client_object = storage.getClientObject();
        if (!client_object)
        {
            R_ASSERT2(false, "it can't be, something goes wrong!");
            return false;
        }

        object_vertex = Globals::Game::get_game_graph()->vertex(client_object->game_vertex_id());
        object_position = client_object->Position();
    }

    const CGameGraph::CVertex* smart_vertex = Globals::Game::get_game_graph()->vertex(smart->m_tGraphID);

    if (object->m_group_id)
    {
        Script_SE_SimulationSquad* squad = nullptr;

        if (Script_SimulationBoard::getInstance().getSquads().find(object->m_group_id) !=
            Script_SimulationBoard::getInstance().getSquads().end())
            squad = Script_SimulationBoard::getInstance().getSquads().at(object->m_group_id);

        if (squad && !squad->getCurrentAction().getName().empty())
        {
            if (squad->getCurrentAction().getName() == Globals::kSimulationSquadCurrentActionIDReachTarget)
            {
                CSE_ALifeDynamicObject* squad_target = nullptr;
                if (Script_SimulationObjects::getInstance().getObjects().find(squad->getAssignedTargetID()) !=
                    Script_SimulationObjects::getInstance().getObjects().end())
                    squad_target =
                        Script_SimulationObjects::getInstance().getObjects().at(squad->getAssignedTargetID());

                if (squad_target)
                {
                    if (squad_target->cast_script_se_simulationsquad())
                        return squad_target->cast_script_se_simulationsquad()->am_i_reached();

                    if (squad_target->cast_script_se_actor())
                        return squad_target->cast_script_se_actor()->am_i_reached();

                    if (squad_target->cast_script_se_smartterrain())
                        return squad_target->cast_script_se_smartterrain()->am_i_reached(squad);

                    R_ASSERT2(false, "not reached!");
                    return false;
                }
                else
                {
                    CSE_ALifeDynamicObject* server_object = ai().alife().objects().object(squad->getAssignedTargetID());
                    if (server_object->cast_script_se_simulationsquad())
                        return server_object->cast_script_se_simulationsquad()->am_i_reached();

                    if (server_object->cast_script_se_actor())
                        return server_object->cast_script_se_actor()->am_i_reached();

                    if (server_object->cast_script_se_smartterrain())
                        return server_object->cast_script_se_smartterrain()->am_i_reached(squad);

                    R_ASSERT2(false, "not reached!");
                    return false;
                }
            }
            else if (squad->getCurrentAction().getName() == "stay_point")
            {
                return true;
            }
        }
    }

    if (object_vertex->level_id() == smart_vertex->level_id())
    {
        return object_position.distance_to_sqr(smart->position()) <= 10000;
    }
    else
    {
        return false;
    }

    return false;
}

bool is_job_available_to_npc(const NpcInfo& npc_info, const JobData_SubData& job_info, Script_SE_SmartTerrain* smart)
{
    if (!smart)
    {
        R_ASSERT2(false, "object was null!");
        return false;
    }

    // @ Lord: убедить что будет выполняться оригинальное условие что объект вообще был инициализирован после
    // default инициализации, то есть xrTime() -> xrTime().setSomeValue();
    if (smart->getDeadTime()[job_info.m_job_index] > 0)
        return false;

    if (job_info.m_function)
    {
        if (!job_info.m_function(npc_info.m_server_object, smart, job_info.m_function_params, npc_info))
            return false;
    }

    return true;
}

bool is_job_available_to_npc(const NpcInfo& npc_info, JobDataExclusive* job_info, Script_SE_SmartTerrain* smart)
{
    if (!smart)
    {
        R_ASSERT2(false, "object was null!");
        return false;
    }

    if (smart->getDeadTime()[job_info->m_job_index] > 0)
        return false;

    if (job_info->m_function)
    {
        if (job_info->m_function(npc_info.m_server_object, smart, job_info->m_function_params))
            return false;
    }

    return true;
}

inline void job_iterator(std::pair<xr_vector<JobData>, xr_vector<JobDataExclusive*>>& jobs, NpcInfo& npc_info,
    std::uint32_t selected_job_priority, Script_SE_SmartTerrain* smart, std::uint32_t& result_id,
    JobData_SubData* result_link, JobDataExclusive* result_exclusive, std::uint32_t& result_priority)
{
    if (!smart)
    {
        R_ASSERT2(false, "object was null!");
        return;
    }

    result_priority = selected_job_priority;
    result_link = nullptr;
    result_exclusive = nullptr;
    result_id = 0;

    for (JobData& it : jobs.first)
    {
        for (std::pair<std::uint32_t, xr_vector<JobData_SubData>>& it_sub : it.m_jobs)
        {
            for (JobData_SubData& it_job : it_sub.second)
            {
                if (result_priority > it_job.m_priority)
                    return;

                if (is_job_available_to_npc(npc_info, it_job, smart))
                {
                    if (!it_job.m_npc_id)
                    {
                        result_priority = it_job.m_priority;
                        result_link = &it_job;
                        result_id = it_job.m_job_index;
                        return;
                    }
                    else if (it_job.m_job_index == npc_info.m_job_id)
                    {
                        result_priority = it_job.m_priority;
                        result_link = &it_job;
                        result_id = it_job.m_job_index;
                        return;
                    }
                }
            }
        }
        // @ Потому что следующая работа отведена под монстров но мы их не учитываем потому что is_job_available_to_npc
        // всегда false
        break;
    }

    for (JobDataExclusive* it : jobs.second)
    {
        if (result_priority > it->m_priority)
            return;

        if (is_job_available_to_npc(npc_info, it, smart))
        {
            if (!it->m_npc_id)
            {
                if (result_link)
                {
                    R_ASSERT2(false, "it can't be!");
                    return;
                }

                result_priority = it->m_priority;
                result_exclusive = it;
                result_id = it->m_job_index;
                return;
            }
            else if (it->m_job_index == npc_info.m_job_id)
            {
                if (result_link)
                {
                    R_ASSERT2(false, "it can't be!");
                    return;
                }

                result_priority = it->m_priority;
                result_exclusive = it;
                result_id = it->m_job_index;
                return;
            }
        }
    }

    R_ASSERT2(false, "can't be reached!");
    return;
}

} // namespace Scripts
} // namespace Cordis

namespace Cordis
{
namespace Scripts
{
Script_SE_SmartTerrain::Script_SE_SmartTerrain(LPCSTR section)
    : inherited(section), m_is_initialized(false), m_is_registered(false), m_population(0),
      m_smart_showed_spot_name(""), m_is_disabled(false), m_is_respawn_point(true), m_base_on_actor_control(nullptr),
      m_ltx(nullptr)
{
    Msg("[Scripts/Script_SE_SmartTerrain/ctor(section)] %s", section);
}

Script_SE_SmartTerrain::~Script_SE_SmartTerrain(void)
{
    {
        xr_string previous_section_name;
        for (JobDataExclusive* it : this->m_jobs.second)
        {
            it->m_job_id.setDeallocationChecker(previous_section_name);

            previous_section_name = it->m_job_id.m_section_name;
            if (it->m_job_id.m_ini_file)
            {
                Msg("[Scripts/Script_SE_SmartTerrain/~dtor()] deleting m_ini_file from %s",
                    it->m_job_id.m_section_name.c_str());
                xr_delete(it);
            }
        }
    }

    {
        for (std::pair<const std::uint32_t, JobDataSmartTerrain*>& it : this->m_job_data)
        {
            Msg("[Scripts/Script_SE_SmartTerrain/~dtor()] deleting JobDataSmartTerrain from this->m_job_data! %s",
                it.second->m_job_id.first);
            xr_delete(it.second);
        }
    }
}

void Script_SE_SmartTerrain::on_before_register(void)
{
    inherited::on_before_register();
    Script_SimulationBoard::getInstance().register_smart(this);
    this->m_smart_level =
        *(ai().game_graph().header().level(ai().game_graph().vertex(this->m_tGraphID)->level_id()).name());
}

void Script_SE_SmartTerrain::on_register(void)
{
    inherited::on_register();

    Script_StoryObject::getInstance().check_spawn_ini_for_story_id(this);
    Script_SimulationObjects::getInstance().registrate(this);

    Msg("[Scripts/Script_SE_SmartTerrain/on_register()] register smart %s", this->name_replace());

    Msg("[Scripts/Script_SE_SmartTerrain/on_register()] Returning alife task for object [%d] game_vertex [%d] "
        "level_vertex [%d] position %f %f %f",
        this->ID, this->m_tGraphID, this->m_tNodeID, this->o_Position.x, this->o_Position.y, this->o_Position.z);

    this->m_smart_alife_task = std::make_unique<CALifeSmartTerrainTask>(this->m_tGraphID, this->m_tNodeID);

    // Script_GlobalHelper::getInstance().setGameRegisteredServerSmartTerrainsByName(this->name_replace(), this);
    DataBase::Storage::getInstance().setGameRegisteredServerSmartTerrainsByName(this->name_replace(), this);
    this->m_is_registered = true;

    this->load_jobs();

    Script_SimulationBoard::getInstance().init_smart(this);

    if (this->m_is_need_init_npc)
    {
        this->m_is_need_init_npc = false;
        // Lord: реализовать метод
        // this->init_npc_after_load();
    }
}

void Script_SE_SmartTerrain::on_unregister(void) {}

void Script_SE_SmartTerrain::STATE_Read(NET_Packet& packet, u16 size)
{
    inherited::STATE_Read(packet, size);

    if (FS.IsSDK())
        return;

    Globals::set_save_marker(packet, Globals::kSaveMarkerMode_Load, false, "Script_SE_SmartTerrain");
    this->read_params();

    std::uint8_t n = packet.r_u8();

    // Информация о НПС идущих в смарт
    for (std::uint8_t i = 0; i < n; ++i)
    {
        std::uint16_t npc_id = packet.r_u16();
        this->m_arriving_npc[npc_id] = nullptr;
    }

    // ИНформация о НПС в смарте
    n = packet.r_u8();
    for (std::uint16_t i = 0; i < n; ++i)
    {
        std::uint16_t npc_id = packet.r_u16();
        NpcInfo npc_info;
        npc_info.m_job_prioprity = packet.r_u8();
        if (npc_info.m_job_prioprity == Globals::kUnsignedInt8Undefined)
        {
            npc_info.m_job_prioprity = -1;
        }

        npc_info.m_job_id = packet.r_u8();
        if (npc_info.m_job_id == Globals::kUnsignedInt8Undefined)
        {
            npc_info.m_job_id = -1;
        }

        npc_info.m_begin_job = packet.r_u8() ? true : false;
        packet.r_stringZ(npc_info.m_need_job);

        this->m_npc_info[npc_id] = npc_info;
    }

    n = packet.r_u8();

    for (std::uint16_t i = 0; i < n; ++i)
    {
        std::uint8_t job_id = packet.r_u8();
        xrTime time = Globals::Utils::r_CTime(packet);
        this->m_dead_time[job_id] = time;
    }

    this->m_is_need_init_npc = true;

    if (this->m_script_version > 9)
    {
        if (!!packet.r_u8() == true)
        {
            this->m_base_on_actor_control->load(packet);
        }
    }

    bool is_respawn_point = packet.r_u8() ? true : false;

    if (is_respawn_point)
    {
        n = packet.r_u8();

        for (std::uint16_t i = 0; i < n; ++i)
        {
            xr_string id;
            packet.r_stringZ(id);
            std::uint8_t num = packet.r_u8();

            this->m_already_spawned[id] = num;
        }

        if (this->m_script_version > 11)
        {
            bool is_exist = packet.r_u8() ? true : false;

            if (is_exist)
            {
                this->m_last_respawn_update = Globals::Utils::r_CTime(packet);
            }
            else
            {
                this->m_last_respawn_update = xrTime();
                Msg("[Scripts/Script_SE_SmartTerrain/STATE_Read(packet, size)] this->m_last_respawn_update = "
                    "xrTime()!");
            }
        }
    }

    this->m_population = packet.r_u8();

    Globals::set_save_marker(packet, Globals::kSaveMarkerMode_Load, true, "Script_SE_SmartTerrain");
}

void Script_SE_SmartTerrain::STATE_Write(NET_Packet& packet)
{
    inherited::STATE_Write(packet);

    Globals::set_save_marker(packet, Globals::kSaveMarkerMode_Save, false, "Script_SE_SmartTerrain");

    packet.w_u8(this->m_arriving_npc.size());

    for (const auto& it : this->m_arriving_npc)
        packet.w_u16(it.first);

    packet.w_u8(this->m_npc_info.size());

    for (const auto& it : this->m_npc_info)
    {
        packet.w_u16(it.first);
        packet.w_u8(it.second.m_job_prioprity);
        packet.w_u8(it.second.m_job_id);
        packet.w_u8(it.second.m_begin_job ? true : false);
        packet.w_stringZ(it.second.m_need_job.c_str());
    }

    packet.w_u8(this->m_dead_time.size());

    for (auto& it : this->m_dead_time)
    {
        packet.w_u8(it.first);
        Globals::Utils::w_CTime(packet, it.second);
    }

    if (this->m_base_on_actor_control)
    {
        packet.w_u8(1);
        this->m_base_on_actor_control->save(packet);
    }
    else
    {
        packet.w_u8(0);
    }

    if (this->m_is_respawn_point)
    {
        packet.w_u8(1);
        packet.w_u8(this->m_already_spawned.size());

        for (auto& it : this->m_already_spawned)
        {
            packet.w_stringZ(it.first.c_str());
            packet.w_u8(it.second);
        }

        if (!(this->m_last_respawn_update == xrTime()))
        {
            packet.w_u8(1);
            Globals::Utils::w_CTime(packet, this->m_last_respawn_update);
        }
        else
        {
            packet.w_u8(0);
        }
    }
    else
    {
        packet.w_u8(0);
    }

    if (this->m_population < 0)
    {
        R_ASSERT2(false, "can't be less than zero!");
    }

    packet.w_u8(this->m_population);

    Globals::set_save_marker(packet, Globals::kSaveMarkerMode_Save, true, "Script_SE_SmartTerrain");
}

void Script_SE_SmartTerrain::register_npc(CSE_ALifeMonsterAbstract* object)
{
    if (!object)
    {
        R_ASSERT2(false, "can't register nullptr!");
        return;
    }

    Msg("[Scripts/Script_SE_SmartTerrain/register_npc(object)] register object %s", object->name_replace());

    ++(this->m_population);

    if (!this->m_is_registered)
    {
        this->m_npc_to_register.push_back(object);
    }

    if (!Globals::IsStalker(object))
    {
        object->m_task_reached = true;
    }

    object->m_smart_terrain_id = this->ID;

    if (arrived_to_smart(object, this))
    {
        this->m_npc_info[object->ID] = this->fill_npc_info(object);
    }
    else
    {
        this->m_arriving_npc[object->ID] = object;
    }
}

void Script_SE_SmartTerrain::unregister_npc(CSE_ALifeMonsterAbstract* object)
{
    if (!object)
    {
        R_ASSERT2(false, "can't use an empty object!");
        return;
    }

    --(this->m_population);

    if (this->m_npc_info[object->ID].m_server_object)
    {
        object->m_smart_terrain_id = Globals::kUnsignedInt16Undefined;
        return;
    }

    if (this->m_arriving_npc[object->ID])
    {
        this->m_arriving_npc[object->ID] = nullptr;
        object->m_smart_terrain_id = Globals::kUnsignedInt16Undefined;
        return;
    }

    R_ASSERT2(false, "not reached!");
}

CALifeSmartTerrainTask* Script_SE_SmartTerrain::task(CSE_ALifeMonsterAbstract* object)
{
    if (!object)
    {
        R_ASSERT2(false, "something goes wrong, can't be!");
        return nullptr;
    }

    if (this->m_arriving_npc[object->ID])
        return this->m_smart_alife_task.get();

    return this->m_job_data[this->m_npc_info[object->ID].m_job_id]->m_alife_task;
}

void Script_SE_SmartTerrain::read_params(void)
{
    CInifile& ini = this->spawn_ini();
    if (!ini.section_exist(Globals::kSmartTerrainSMRTSection))
    {
        R_ASSERT2(false, "YOU LOGIC MUST CONTAIN SECTION [smart_terrain]");
        this->m_is_disabled = true;
        return;
    }

    xr_string filename = Globals::Utils::cfg_get_string(&ini, Globals::kSmartTerrainSMRTSection, "cfg");
    if (!filename.empty())
    {
        if (FS.exist("$game_config$", filename.c_str()))
        {
            this->m_ini = std::make_unique<CScriptIniFile>(filename.c_str());
        }
        else
        {
            R_ASSERT2(false, "YOU MUST REGISTRATE CONFIGURATION FILE OF YOUR SMART TERRAIN");
            this->m_is_disabled = true;
            return;
        }
    }

    CScriptIniFile* script_ini = this->getIni();

    if (script_ini)
        this->m_simulation_type_name =
            Globals::Utils::cfg_get_string(script_ini, Globals::kSmartTerrainSMRTSection, "sim_type");
    else
        this->m_simulation_type_name = "default";

    if (this->m_simulation_type_name.empty())
        this->m_simulation_type_name = "default";

    if (Script_GlobalHelper::getInstance().getRegisteredSmartTerrainTerritoryType().find(
            this->m_simulation_type_name) ==
        Script_GlobalHelper::getInstance().getRegisteredSmartTerrainTerritoryType().end())
    {
        Msg("[Scripts/Script_SE_SmartTerrain/read_params()] ERROR: %s", this->m_simulation_type_name.c_str());
        R_ASSERT2(false, "Invalid type of territory check the getRegisteredSmartTerrainTerritoryType()!");
    }

    if (script_ini)
        this->m_squad_id = static_cast<std::uint16_t>(
            Globals::Utils::cfg_get_number(script_ini, Globals::kSmartTerrainSMRTSection, "squad_id"));
    else
        this->m_squad_id = 0;

    xr_string respawn_sector_name;
    if (script_ini)
        respawn_sector_name =
            Globals::Utils::cfg_get_string(script_ini, Globals::kSmartTerrainSMRTSection, "respawn_sector");

    if (script_ini)
        this->m_respawn_radius = static_cast<std::uint16_t>(
            Globals::Utils::cfg_get_number(script_ini, Globals::kSmartTerrainSMRTSection, "respawn_radius"));
    else
        this->m_respawn_radius = 150;

    if (!respawn_sector_name.empty())
    {
        if (respawn_sector_name == "default")
        {
            respawn_sector_name = "all";
        }

        this->m_respawn_sector_condlist = XR_LOGIC::parse_condlist_by_server_object(
            Globals::kSmartTerrainSMRTSection, "respawn_sector", respawn_sector_name);
    }

    this->m_is_mutant_lair = script_ini ?
        Globals::Utils::cfg_get_bool(script_ini, Globals::kSmartTerrainSMRTSection, "mutant_lair") :
        Globals::Utils::cfg_get_bool(&ini, Globals::kSmartTerrainSMRTSection, "mutant_lair");
    this->m_is_no_mutant = script_ini ?
        Globals::Utils::cfg_get_bool(script_ini, Globals::kSmartTerrainSMRTSection, "no_mutant") :
        Globals::Utils::cfg_get_bool(&ini, Globals::kSmartTerrainSMRTSection, "no_mutant");

    if (this->m_is_no_mutant)
        Msg("[Scripts/Script_SE_SmartTerrain/read_params()] Found no mutant point %s", this->name_replace());

    this->m_fobidden_point_name = script_ini ?
        Globals::Utils::cfg_get_string(script_ini, Globals::kSmartTerrainSMRTSection, "forbidden_point") :
        Globals::Utils::cfg_get_string(&ini, Globals::kSmartTerrainSMRTSection, "forbidden_point");

    this->m_defence_restictor = script_ini ?
        Globals::Utils::cfg_get_string(script_ini, Globals::kSmartTerrainSMRTSection, "def_restr") :
        Globals::Utils::cfg_get_string(&ini, Globals::kSmartTerrainSMRTSection, "def_restr");
    this->m_attack_restrictor = script_ini ?
        Globals::Utils::cfg_get_string(script_ini, Globals::kSmartTerrainSMRTSection, "att_restr") :
        Globals::Utils::cfg_get_string(&ini, Globals::kSmartTerrainSMRTSection, "att_restr");
    this->m_safe_restirctor = script_ini ?
        Globals::Utils::cfg_get_string(script_ini, Globals::kSmartTerrainSMRTSection, "safe_restr") :
        Globals::Utils::cfg_get_string(&ini, Globals::kSmartTerrainSMRTSection, "safe_restr");
    this->m_spawn_point_name = script_ini ?
        Globals::Utils::cfg_get_string(script_ini, Globals::kSmartTerrainSMRTSection, "spawn_point") :
        Globals::Utils::cfg_get_string(&ini, Globals::kSmartTerrainSMRTSection, "spawn_point");
    this->m_arrive_distance = static_cast<std::uint32_t>(script_ini ?
            Globals::Utils::cfg_get_number(script_ini, Globals::kSmartTerrainSMRTSection, "arrive_dist") :
            Globals::Utils::cfg_get_number(&ini, Globals::kSmartTerrainSMRTSection, "arrive_dist"));

    if (this->m_arrive_distance < 0)
        this->m_arrive_distance = 30;

    xr_string max_population_name = script_ini ?
        Globals::Utils::cfg_get_string(script_ini, Globals::kSmartTerrainSMRTSection, "max_population") :
        Globals::Utils::cfg_get_string(&ini, Globals::kSmartTerrainSMRTSection, "max_population");
    xr_map<std::uint32_t, CondlistData> parsed_condlist = XR_LOGIC::parse_condlist_by_server_object(
        Globals::kSmartTerrainSMRTSection, "max_population", max_population_name);

    this->m_max_population = atoi(XR_LOGIC::pick_section_from_condlist(
        Globals::get_story_object("actor"), (CSE_ALifeDynamicObject*)(nullptr), parsed_condlist)
                                      .c_str());

    xr_string respawn_params = script_ini ?
        Globals::Utils::cfg_get_string(script_ini, Globals::kSmartTerrainSMRTSection, "respawn_params") :
        Globals::Utils::cfg_get_string(&ini, Globals::kSmartTerrainSMRTSection, "respawn_params");
    this->m_is_respawn_only_smart = script_ini ?
        Globals::Utils::cfg_get_bool(script_ini, Globals::kSmartTerrainSMRTSection, "respawn_only_smart") :
        Globals::Utils::cfg_get_bool(&ini, Globals::kSmartTerrainSMRTSection, "respawn_only_smart");

    xr_string smart_control_section_name = script_ini ?
        Globals::Utils::cfg_get_string(script_ini, Globals::kSmartTerrainSMRTSection, "smart_control") :
        Globals::Utils::cfg_get_string(&ini, Globals::kSmartTerrainSMRTSection, "smart_control");

    if (!(smart_control_section_name.empty()))
    {
        if (this->m_base_on_actor_control)
        {
            R_ASSERT2(false, "something goes wrong. The object must be empty (NULL)!!");
        }

        if (script_ini)
            this->m_base_on_actor_control =
                std::make_unique<Script_SmartTerrainControl>(this, *script_ini, smart_control_section_name);
        else
            this->m_base_on_actor_control =
                std::make_unique<Script_SmartTerrainControl>(this, this->spawn_ini(), smart_control_section_name);
    }

    this->m_is_respawn_point = false;

    if (!(respawn_params.empty()))
    {
        this->check_respawn_params(respawn_params);
    }

    xr_string traveller_actor_name = this->name_replace();
    traveller_actor_name += "_traveller_actor";

    xr_string traveller_squad_name = this->name_replace();
    traveller_squad_name += "_traveller_squad";

    if (Globals::patrol_path_exists(traveller_actor_name.c_str()))
    {
        Msg("[Scripts/Script_SE_SmartTerrain/read_params()] %s has no traveller path traveller_actor_name",
            this->name_replace());
        this->m_traveller_actor_path_name = traveller_actor_name;
    }

    if (Globals::patrol_path_exists(traveller_squad_name.c_str()))
    {
        Msg("[Scripts/Script_SE_SmartTerrain/read_params()] %s has no traveller path traveller_squad_name",
            this->name_replace());
        this->m_traveller_squad_path_name = traveller_squad_name;
    }

    if (!ini_file_locations.section_exist(this->name_replace()))
    {
        Msg("[Scripts/Script_SE_SmartTerrain/read_params()] %s has no terrain_mask section in smart_terrain_masks.ltx!",
            this->name_replace());
    }
}

void Script_SE_SmartTerrain::on_after_reach(Script_SE_SimulationSquad* squad)
{
    if (!squad)
    {
        R_ASSERT2(false, "object was null!");
        return;
    }

    for (AssociativeVector<std::uint16_t, CSE_ALifeMonsterAbstract*>::const_iterator it =
             squad->squad_members().begin();
         it != squad->squad_members().end(); ++it)
    {
        if ((*it).second)
        {
            Script_SimulationBoard::getInstance().setup_squad_and_group((*it).second->cast_alife_dynamic_object());
        }
    }

    squad->setCurrentTargetID(this->ID);
}

void Script_SE_SmartTerrain::on_reach_target(Script_SE_SimulationSquad* squad)
{
    if (!squad)
    {
        R_ASSERT2(false, "object was null!");
        return;
    }

    squad->set_location_types(this->name_replace());

    Script_SimulationBoard::getInstance().assigned_squad_to_smart(squad, this->ID);

    for (AssociativeVector<std::uint16_t, CSE_ALifeMonsterAbstract*>::const_iterator it =
             squad->squad_members().begin();
         it != squad->squad_members().end(); ++it)
    {
        if (DataBase::Storage::getInstance().getOfflineObjects().at(it->first).second.size())
        {
            DataBase::Storage::getInstance().setOfflineObjects(it->first, Globals::kUnsignedInt16Undefined, "");
        }
    }
}

void Script_SE_SmartTerrain::clear_dead(CSE_ALifeDynamicObject* server_object)
{
    if (!server_object)
    {
        R_ASSERT2(false, "object was nullptr!");
        return;
    }

    if (this->m_npc_info[server_object->ID].m_job_link1 && this->m_npc_info[server_object->ID].m_job_link2)
    {
        R_ASSERT2(false, "CANNOT BE SOMETHING WRONG!");
        return;
    }

    if (this->m_npc_info[server_object->ID].m_job_link1 ? this->m_npc_info[server_object->ID].m_job_link1->m_job_index :
                                                          this->m_npc_info[server_object->ID].m_job_link2->m_job_index)
    {
        std::uint32_t m_job_index = this->m_npc_info[server_object->ID].m_job_link1 ?
            this->m_npc_info[server_object->ID].m_job_link1->m_job_index :
            this->m_npc_info[server_object->ID].m_job_link2->m_job_index;
        this->m_dead_time[m_job_index] = Globals::Game::get_game_time();
        this->m_npc_info[server_object->ID].clear();
        CSE_ALifeMonsterAbstract* object = server_object->cast_monster_abstract();

        if (!object)
        {
            R_ASSERT2(false, "bad cast!");
            return;
        }

        object->m_smart_terrain_id = 0xffff;
        return;
    }
    else
    {
        R_ASSERT2(false, "something went wrong! Can't be here that this->m_npc_info = unInitialize");
        return;
    }

    if (this->m_arriving_npc[server_object->ID])
    {
        this->m_arriving_npc[server_object->ID] = nullptr;
        CSE_ALifeMonsterAbstract* object = server_object->cast_monster_abstract();

        if (!object)
        {
            R_ASSERT2(false, "bad cast!");
            return;
        }

        object->m_smart_terrain_id = 0xffff;
        //   return;
    }
}

void Script_SE_SmartTerrain::hide(void)
{
    if (!this->m_smart_showed_spot_name.size())
        return;

    xr_string spot_type_name = "alife_presentation_smart_";
    spot_type_name += this->m_simulation_type_name;
    spot_type_name += "_";
    spot_type_name += this->m_smart_showed_spot_name;

    Globals::Game::level::map_remove_object_spot(this->ID, spot_type_name.c_str());
}

void Script_SE_SmartTerrain::check_respawn_params(xr_string& params)
{
    if (params.empty())
    {
        Msg("[Scripts/Script_SE_SmartTerrain/check_respawn_params(params)] WARNING: params.empty() == true! Return");
        return;
    }

    if (this->getIni())
    {
        if (!this->getIni()->section_exist(params.c_str()))
        {
            R_ASSERT2(false, "INVALID: Wrong smart_terrain respawn_params section!");
            return;
        }
    }
    else
    {
        if (!this->spawn_ini().section_exist(params.c_str()))
        {
            R_ASSERT2(false, "INVALID: Wrong smart_terrain respawn_params section!");
            return;
        }
    }

    std::uint32_t n =
        this->getIni() ? this->getIni()->line_count(params.c_str()) : this->spawn_ini().line_count(params.c_str());

    if (!n)
    {
        R_ASSERT2(false, "Wrong smart_terrain empty params!");
    }

    for (std::uint32_t i = 0; i < n; ++i)
    {
        const char* prop_name;
        const char* prop_condlist;
        bool status = this->getIni() ? this->getIni()->r_line(params.c_str(), i, &prop_name, &prop_condlist) :
                                       this->spawn_ini().r_line(params.c_str(), i, &prop_name, &prop_condlist);

        if (this->getIni())
        {
            if (!this->getIni()->section_exist(prop_name))
            {
                R_ASSERT2(false, "Wrong smart_terrain params no section!");
                return;
            }
        }
        else
        {
            if (!this->spawn_ini().section_exist(prop_name))
            {
                R_ASSERT2(false, "Wrong smart_terrain params no section!");
                return;
            }
        }

        xr_string spawn_squads;
        xr_string spawn_num;
        spawn_squads = this->getIni() ? Globals::Utils::cfg_get_string(this->getIni(), prop_name, "spawn_squads") :
                                        Globals::Utils::cfg_get_string(this->spawn_ini(), prop_name, "spawn_squads");

        spawn_num = this->getIni() ? Globals::Utils::cfg_get_string(this->getIni(), prop_name, "spawn_num") :
                                     Globals::Utils::cfg_get_string(this->spawn_ini(), prop_name, "spawn_num");

        if (spawn_squads.empty())
        {
            R_ASSERT2(false, "Something goes worng!~");
            return;
        }
        else if (spawn_num.empty())
        {
            R_ASSERT2(false, "Something goes wrong!");
            return;
        }

        xr_vector<xr_string> parsed_squads_name = Globals::Utils::parse_names(spawn_squads);
        xr_map<std::uint32_t, CondlistData> parsed_condlist =
            XR_LOGIC::parse_condlist_by_server_object(prop_name, "spawn_num", spawn_num);

        this->m_respawn_params[prop_name] =
            std::pair<xr_vector<xr_string>, xr_map<std::uint32_t, CondlistData>>(parsed_squads_name, parsed_condlist);
        this->m_already_spawned[prop_name] = 0;
    }
}

void Script_SE_SmartTerrain::select_npc_job(NpcInfo& npc_info)
{
    JobData_SubData* selected_job_link = nullptr;
    JobDataExclusive* selected_job_link_exclusive = nullptr;
    std::uint32_t selected_priority = 0;
    std::uint32_t selected_job_index = 0;

    job_iterator(this->m_jobs, npc_info, 0, this, selected_job_index, selected_job_link, selected_job_link_exclusive,
        selected_priority);

    if (!selected_job_index)
    {
        R_ASSERT2(false, "it can't be!");
        return;
    }

    if (selected_job_link && selected_job_link_exclusive)
    {
        R_ASSERT2(false, "it can't be!");
        return;
    }

    // Назначаем работу!
    if (selected_job_index != npc_info.m_job_id && (selected_job_link || selected_job_link_exclusive))
    {
        if (npc_info.m_job_link1 || npc_info.m_job_link2)
        {
            this->m_npc_by_job_section[this->m_job_data[npc_info.m_job_link1 ? npc_info.m_job_link1->m_job_index :
                                                                               npc_info.m_job_link2->m_job_index]
                                           ->m_job_id.first] = 0;
            if (npc_info.m_job_link1)
            {
                npc_info.m_job_link1 = nullptr;
                if (npc_info.m_job_link2)
                {
                    R_ASSERT2(false, "can't be!");
                    return;
                }
            }
            else
            {
                if (!npc_info.m_job_link2)
                {
                    R_ASSERT2(false, "it can't be!");
                    return;
                }

                npc_info.m_job_link2 = nullptr;
            }
        }

        if (selected_job_link)
            selected_job_link->m_npc_id = npc_info.m_server_object->ID;

        if (selected_job_link_exclusive)
            selected_job_link_exclusive->m_npc_id = npc_info.m_server_object->ID;

        this->m_npc_by_job_section[this->m_job_data[npc_info.m_job_link1 ? npc_info.m_job_link1->m_job_index :
                                                                           npc_info.m_job_link2->m_job_index]
                                       ->m_job_id.first] =
            selected_job_link ? selected_job_link->m_npc_id : selected_job_link_exclusive->m_npc_id;

        npc_info.m_job_id =
            selected_job_link ? selected_job_link->m_job_index : selected_job_link_exclusive->m_job_index;
        npc_info.m_job_prioprity =
            selected_job_link ? selected_job_link->m_priority : selected_job_link_exclusive->m_priority;
        npc_info.m_begin_job = false;

        if (selected_job_link)
            npc_info.m_job_link1 = selected_job_link;

        if (selected_job_link_exclusive)
            npc_info.m_job_link2 = selected_job_link_exclusive;
    }
}

void Script_SE_SmartTerrain::show(void)
{
    std::uint32_t time = Device.dwTimeGlobal;

    if (this->m_show_time && this->m_show_time + 200 >= time)
        return;

    xr_string player = this->m_player_name;
    xr_string spot_name = Globals::kRelationsTypeNeutral;

    if (!this->IsSimulationAvailable() &&
        XR_LOGIC::pick_section_from_condlist(DataBase::Storage::getInstance().getActor(), this,
            this->getSimulationAvail()) == XR_LOGIC::kXRLogicReturnTypeSuccessfulName)
        spot_name = "friend";
    else
        spot_name = "enemy";

    if (this->m_smart_showed_spot_name == spot_name)
    {
        xr_string spot_type_name = "alife_presentation_smart_";
        spot_type_name += this->m_simulation_type_name;
        spot_type_name += "_";
        spot_type_name += this->m_smart_showed_spot_name;
        Globals::Game::level::map_change_spot_hint(this->ID, spot_type_name.c_str(), "");
        return;
    }

    xr_string spot_type_name = "alife_presentation_smart_";
    spot_type_name += this->m_simulation_type_name;
    spot_type_name += "_";
    spot_type_name += this->m_smart_showed_spot_name;

    if (this->m_smart_showed_spot_name.size() &&
        Globals::Game::level::map_has_object_spot(this->ID, spot_type_name.c_str()))
    {
        xr_string spot_type_name2 = "alife_presentation_smart_base_";
        spot_type_name2 += this->m_smart_showed_spot_name;
        Globals::Game::level::map_remove_object_spot(this->ID, spot_type_name2.c_str());
    }
}

void Script_SE_SmartTerrain::load_jobs(void)
{
    this->m_jobs = GulagGenerator::load_job(this);
    this->m_ltx = XR_GULAG::loadLtx(this->name_replace());
    auto sort_function = [](const std::pair<std::uint32_t, xr_vector<JobData_SubData>>& a,
                             const std::pair<std::uint32_t, xr_vector<JobData_SubData>>& b) -> bool {
        for (const JobData_SubData& it : a.second)
        {
            if (it.m_job_id.second.empty())
            {
                R_ASSERT2(false, "job_type can't be empty check Script_GulagGenerator!");
                return false;
            }

            if (it.m_job_id.first.empty())
            {
                R_ASSERT2(false, "section_name can't be empty check Script_GulagGenerator!");
                return false;
            }
        }

        return (a.first > b.first);
    };

    auto sort_exclusive_function = [](JobDataExclusive* a, JobDataExclusive* b) -> bool {
        if (!a)
        {
            R_ASSERT2(false, "something goes wrong!");
            return false;
        }

        if (!b)
        {
            R_ASSERT2(false, "something goes wrong!");
            return false;
        }

        if (a->m_job_id.m_section_name.empty())
        {
            R_ASSERT2(false, "section_name can't be empty! Bad initialization check Script_GulagGenerator!");
            return false;
        }

        if (b->m_job_id.m_section_name.empty())
        {
            R_ASSERT2(false, "section_name can't be empty! Bad initialization check Script_GulagGenerator!");
            return false;
        }

        if (a->m_job_id.m_job_type.empty())
        {
            R_ASSERT2(false, "job_type can't be empty! Bad initialization check Script_GulagGenerator!");
            return false;
        }

        if (b->m_job_id.m_job_type.empty())
        {
            R_ASSERT2(false, "job_type can't be empty! Bad initialization check Script_GulagGenerator!");
            return false;
        }

        return (a->m_priority > b->m_priority);
    };

    // @ Sorting
    {
        for (JobData& it : this->m_jobs.first)
        {
            /*
                        for (std::pair<std::uint32_t, xr_vector<JobData_SubData>>& it_sub : it.m_jobs)
                        {
                            std::sort(it_sub.second.begin(), it_sub.second.end(), sort_function);
                        }*/
            std::sort(it.m_jobs.begin(), it.m_jobs.end(), sort_function);
        }

        std::sort(this->m_jobs.second.begin(), this->m_jobs.second.end(), sort_exclusive_function);
    }

    // @ Initializing m_job_data
    {
        std::uint32_t id = 0;
        for (JobData& it : this->m_jobs.first)
        {
            for (std::pair<std::uint32_t, xr_vector<JobData_SubData>>& it_sub : it.m_jobs)
            {
                for (JobData_SubData& it_data_sub : it_sub.second)
                {
                    JobDataSmartTerrain* data = new JobDataSmartTerrain();
                    data->m_job_id = it_data_sub.m_job_id;
                    data->m_priority = it_data_sub.m_priority;
                    it_data_sub.clear();
                    it_data_sub.m_job_index = id;
                    this->m_job_data[id] = data;
                    ++id;
                }
            }
        }

        for (JobDataExclusive* it : this->m_jobs.second)
        {
            if (!it)
            {
                R_ASSERT2(false, "something goes VERY WRONG!!!!");
                return;
            }

            JobDataSmartTerrain* data = new JobDataSmartTerrain();
            data->m_job_id = std::pair<xr_string, xr_string>(it->m_job_id.m_section_name, it->m_job_id.m_job_type);
            data->m_priority = it->m_priority;
            data->m_ini_path_name = it->m_job_id.m_ini_path_name;
            data->m_ini_file = it->m_job_id.m_ini_file;
            it->m_job_id.clear();
            it->m_job_index = id;
            this->m_job_data[id] = data;
            ++id;
        }
    }

    // @ Computing CAlifeSmartTerrainTask!
    {
        for (std::pair<const std::uint32_t, JobDataSmartTerrain*>& it : this->m_job_data)
        {
            const xr_string& section_name = it.second->m_job_id.first;
            CScriptIniFile* current_ini = it.second->m_ini_file ? it.second->m_ini_file : this->m_ltx;

            if (!current_ini->line_exist(section_name.c_str(), "active"))
            {
                Msg("[Scripts/Script_SE_SmartTerrain/load_jobs()] ERROR: %s", section_name.c_str());
                R_ASSERT2(false, "no 'active' in section");
            }

            xr_string active_section_name = current_ini->r_string(section_name.c_str(), "active");

            Msg("[Scripts/Script_SE_SmartTerrain/load_jobs()] parsed active section name %s",
                active_section_name.c_str());

            const xr_string& job_type_name = it.second->m_job_id.second;

            if (job_type_name == Globals::GulagGenerator::kGulagJobPath)
            {
                xr_string path_field_name;

                for (const xr_string& it : Script_GlobalHelper::getInstance().getRegisteredSmartTerrainPathFileds())
                {
                    if (current_ini->line_exist(active_section_name.c_str(), it.c_str()))
                    {
                        path_field_name = it;
                        break;
                    }
                }

                Msg("[Scripts/Script_SE_SmartTerrain/load_jobs()] parsed path_field_name %s", path_field_name.c_str());

                xr_string _path_name = current_ini->r_string(active_section_name.c_str(), path_field_name.c_str());
                Msg("[Scripts/Script_SE_SmartTerrain/load_jobs()] parsed path_name %s", _path_name.c_str());

                xr_string path_name = this->name_replace();
                path_name += "_";
                path_name += _path_name;

                Msg("[Scripts/Script_SE_SmartTerrain/load_jobs()] generated path_name %s", path_name.c_str());

                if (path_field_name == Globals::kSmartTerrainPathFieldCenterPoint)
                {
                    xr_string patrol_path_name = path_name;
                    patrol_path_name += "_task";
                    if (Globals::patrol_path_exists(patrol_path_name.c_str()))
                    {
                        path_name = patrol_path_name;
                        Msg("[Scripts/Script_SE_SmartTerrain/load_jobs()] current path_name %s", path_name.c_str());
                    }
                }

                Msg("[Scripts/Script_SE_SmartTerrain/load_jobs()] creating (allocating) alife smart terrain task by "
                    "patrol path");
                it.second->m_alife_task = new CALifeSmartTerrainTask(path_name.c_str());
            }
            else if (job_type_name == Globals::GulagGenerator::kGulagJobSmartCover)
            {
                xr_string smart_cover_name = current_ini->r_string(active_section_name.c_str(), "cover_name");

                Script_SE_SmartCover* smart_cover = nullptr;
                if (DataBase::Storage::getInstance().getGameRegisteredServerSmartCovers().find(smart_cover_name) !=
                    DataBase::Storage::getInstance().getGameRegisteredServerSmartCovers().end())
                    smart_cover =
                        DataBase::Storage::getInstance().getGameRegisteredServerSmartCovers().at(smart_cover_name);

                if (!smart_cover)
                {
                    R_ASSERT2(false, "There is an exclusive job with wrong smart cover name!");
                }

                Msg("[Scripts/Script_SE_SmartTerrain/load_jobs()] creating alife smart terrain task by smart cover "
                    "level_vertex_id [%d] game_vertex_id [%d]",
                    smart_cover->m_tNodeID, smart_cover->m_tGraphID);
                it.second->m_alife_task = new CALifeSmartTerrainTask(smart_cover->m_tGraphID, smart_cover->m_tNodeID);
            }
            else if (job_type_name == Globals::GulagGenerator::kGulagJobPoint)
            {
                Msg("[Scripts/Script_SE_SmartTerrain/load_jobs()] creating alife smart terrain task by smart terrain "
                    "level_vertex_id game_vertex_id");
                it.second->m_alife_task = new CALifeSmartTerrainTask(this->m_tGraphID, this->m_tNodeID);
            }

            if (!it.second->m_alife_task)
            {
                R_ASSERT2(false, "it can't be m_alife_task must be allocated!");
            }

            it.second->m_game_vertex_id = it.second->m_alife_task->game_vertex_id();
            it.second->m_level_id = Globals::Game::get_game_graph()->vertex(it.second->m_game_vertex_id)->level_id();
            it.second->m_position = it.second->m_alife_task->position();
        }
    }
}

} // namespace Scripts
} // namespace Cordis
