#pragma once

#include <imgui.h>
#include <imgui_internal.h>

namespace ImGuiExtra
{
    inline void MarkSettingsDirty()
    {
        if( ImGui::GetCurrentContext() && ImGui::GetCurrentContext()->SettingsDirtyTimer <= 0.0f )
        {
            ImGui::GetCurrentContext()->SettingsDirtyTimer = ImGui::GetIO().IniSavingRate;
        }
    }

    inline void AddOrReplaceSettingsHandler( ImGuiSettingsHandler& settings )
    {
        if( auto* existing = ImGui::FindSettingsHandler( settings.TypeName ) )
        {
            *existing = settings;
        }
        else
        {
            ImGui::GetCurrentContext()->SettingsHandlers.push_back( settings );            
        }
    }

    inline bool ScrollCombo( int* comboIndex, int comboCount )
    {
        if( ImGui::IsItemHovered() )
        {
            ImGui::SetItemUsingMouseWheel();

            if( ImGui::GetIO().MouseWheel < 0 && *comboIndex < comboCount - 1 )
            {
                (*comboIndex)++;
                return true;
            }

            if( ImGui::GetIO().MouseWheel > 0 && *comboIndex > 0 )
            {
                (*comboIndex)--;
                return true;
            }
        }
        return false;
    }
}
