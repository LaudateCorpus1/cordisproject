#pragma once

namespace Cordis
{
namespace Scripts
{
class Script_SchemeMobHome : public Script_ISchemeMonster
{
    using inherited_scheme = Script_ISchemeMonster;
public:
    Script_SchemeMobHome(void) = delete;
    Script_SchemeMobHome(CScriptGameObject* const p_client_object, DataBase::Storage_Scheme& storage);
    ~Script_SchemeMobHome(void);

    virtual void reset_scheme(const bool, CScriptGameObject* const p_client_object);
    virtual void deactivate(CScriptGameObject* const p_client_object);
    
 
    // @ PRIVATE uses, in XR_LOGIC
    static inline void add_to_binder(CScriptGameObject* const p_client_object, CScriptIniFile* const p_ini,
        const xr_string& scheme_name, const xr_string& section_name,
        DataBase::Storage_Scheme& storage)
    {
        if (!p_client_object)
        {
            R_ASSERT2(false, "object is null!");
            return;
        }

        if (!p_ini)
        {
            R_ASSERT2(false, "object is null!");
            return;
        }

        Msg("[Scripts/add_to_binder(p_client_object, p_ini, scheme_name, section_name, storage)] added "
            "Script_SchemeMobWalker scheme to binder, name=%s scheme=%s section=%s",
            p_client_object->Name(), scheme_name.c_str(), section_name.c_str());

        Script_ISchemeEntity* action = new Script_SchemeMobHome(p_client_object, storage);
        DataBase::Storage::getInstance().setStorageSchemesActions(
            p_client_object->ID(), action->getSchemeName(), action);
    }

    // @ PRIVATE, uses in XR_LOGIC
    static void set_scheme(CScriptGameObject* const p_client_object, CScriptIniFile* const p_ini,
        const xr_string& scheme_name, const xr_string& section_name, const xr_string& gulag_name);

};
} // namespace Scripts
} // namespace Cordis
