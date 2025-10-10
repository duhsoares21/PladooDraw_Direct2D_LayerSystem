// SurfaceDial.cpp
#include "pch.h"
#include "Constants.h"
#include "SurfaceDial.h"
#include "Tools.h"
#include "Transforms.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.UI.Input.Core.h>
#include <winrt/Windows.System.h>
#include <radialcontrollerinterop.h>

#include <dispatcherqueue.h>           
#include <winrt/base.h>                
#include <iostream>
#include "ToolsAux.h"

using namespace winrt;
using namespace winrt::Windows::UI::Input;
using namespace winrt::Windows::UI::Input::Core;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System;

static RadialController g_controller{ nullptr };
static event_token     g_rotationToken{};
static event_token     g_buttonToken{};

static DispatcherQueueController g_dispatcherController{ nullptr };

static void EnsureDispatcherQueue()
{
    if (DispatcherQueue::GetForCurrentThread() != nullptr)
        return;

    DispatcherQueueOptions options{};
    options.dwSize = sizeof(DispatcherQueueOptions);
    options.threadType = DQTYPE_THREAD_CURRENT;      
    options.apartmentType = DQTAT_COM_STA;           

    winrt::com_ptr<ABI::Windows::System::IDispatcherQueueController> controllerNative;
    HRESULT hr = CreateDispatcherQueueController(options, controllerNative.put());
    if (FAILED(hr)) {
        return;
    }

    g_dispatcherController = controllerNative.as<DispatcherQueueController>();
}

void InitializeSurfaceDial(HWND hwnd)  
{  
   winrt::init_apartment(winrt::apartment_type::single_threaded);  
    
    EnsureDispatcherQueue();  
    
    auto interop = get_activation_factory<RadialController, IRadialControllerInterop>();  
    com_ptr<::IUnknown> unk;  
    check_hresult(interop->CreateForWindow(  
        hwnd,  
        guid_of<RadialController>(),  
        unk.put_void()  
    ));  
    g_controller = unk.as<RadialController>();  
    auto menu = g_controller.Menu();

    static std::vector<RadialControllerMenuItem> g_menuItems;
        

    g_menuItems.clear();

    if (isReplayMode == 1) {
        auto Navigate = RadialControllerMenuItem::CreateFromKnownIcon(L"Navigate", RadialControllerMenuKnownIcon::NextPreviousTrack);
        g_menuItems.push_back(Navigate);
    }
    else {
        auto zoom = RadialControllerMenuItem::CreateFromKnownIcon(L"Zoom", RadialControllerMenuKnownIcon::Zoom);
        auto brushSize = RadialControllerMenuItem::CreateFromKnownIcon(L"Brush Size", RadialControllerMenuKnownIcon::InkThickness);

        g_menuItems.push_back(zoom);
        g_menuItems.push_back(brushSize);
    }

    menu.Items().Clear();

    for (const auto& item : g_menuItems) {
        menu.Items().Append(item);
    }
    
    if (isReplayMode == 1) {
        g_controller.RotationResolutionInDegrees(1.0);
    }
    else {
        g_controller.RotationResolutionInDegrees(0.3);
    }
    
    g_controller.UseAutomaticHapticFeedback(false);

    g_rotationToken = g_controller.RotationChanged(  
    [](auto&&, RadialControllerRotationChangedEventArgs const& args) {  
        int ticks = (args.RotationDeltaInDegrees() > 0) ? 1 : (args.RotationDeltaInDegrees() < 0) ? -1 : 0;
                       
        auto selected = g_controller.Menu().GetSelectedMenuItem();
        int menuIndex = -1;
        for (size_t i = 0; i < g_menuItems.size(); ++i) {
            if (selected == g_menuItems[i]) {
                menuIndex = static_cast<int>(i);
                break;
            }
        }

        OnDialRotation(menuIndex, ticks, static_cast<float>(args.RotationDeltaInDegrees()));
    });

    g_buttonToken = g_controller.ButtonClicked(  
    [](auto&&, RadialControllerButtonClickedEventArgs const&) {  
            
        auto selected = g_controller.Menu().GetSelectedMenuItem();
        int menuIndex = -1;
        for (size_t i = 0; i < g_menuItems.size(); ++i) {
            if (selected == g_menuItems[i]) {
                menuIndex = static_cast<int>(i);
                break;
            }
        }

        OnDialButtonClick(menuIndex);  
    });  
}

/* SURFACE DIAL */

void OnDialRotation(int menuIndex, int direction, float rotationDegrees) {
    if (direction == 0) return;

    if (menuIndex == 0) {
        if (isReplayMode == 1) {
            if (direction > 0) {
                TReplayForward();
            }
            else {
                TReplayBackwards();
            }
        }
        else {
            if (direction > 0) {
                TZoomIn(rotationDegrees * 0.1f);
            }
            else {
                TZoomOut((rotationDegrees * 0.1f) * -1.0f);
            }
        }
    }
    else if (menuIndex == 1) {
        if (direction > 0) {
            TIncreaseBrushSize(rotationDegrees * 0.1f);
        }
        else {
            TDecreaseBrushSize((rotationDegrees * 0.1f) * -1.0f);
        }
    }
}

int command = 0;
void OnDialButtonClick(int menuIndex) {
    if (menuIndex == 0) {
        if (isReplayMode == 1) {
            KillTimer(replayHWND, 1);
            KillTimer(replayHWND, 2);
            KillTimer(replayHWND, 3);

            if (command == 2) {
                command = 0;
            }
            else {
                SetTimer(replayHWND, 2, 300, NULL);
                command = 2;
            }
        }
        else {
            zoomFactor = 1.0f;
            TZoom();
        }
    }
    else if (menuIndex == 1) {
        if (selectedTool == TBrush) {
            currentBrushSize = defaultBrushSize;
        }
        else if (selectedTool == TEraser) {
            currentEraserSize = defaultEraserSize;
        }
    }
}

void CleanupSurfaceDial()
{
    if (g_controller)
    {
        g_controller.RotationChanged(g_rotationToken);
        g_controller.ButtonClicked(g_buttonToken);
        g_controller = nullptr;
    }

    uninit_apartment();
}