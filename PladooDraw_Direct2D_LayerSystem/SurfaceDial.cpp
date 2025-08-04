// SurfaceDial.cpp
#include "pch.h"
#include "SurfaceDial.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.UI.Input.Core.h>
#include <winrt/Windows.System.h>
#include <radialcontrollerinterop.h>

#include <dispatcherqueue.h>           
#include <winrt/base.h>                
#include <iostream>

using namespace winrt;
using namespace Windows::UI::Input;
using namespace Windows::UI::Input::Core;
using namespace Windows::Foundation;
using namespace Windows::System;

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

    static std::vector<RadialControllerMenuItem> g_menuItems;
        
    auto zoom = RadialControllerMenuItem::CreateFromKnownIcon(L"Zoom", RadialControllerMenuKnownIcon::Zoom);
    auto brushSize = RadialControllerMenuItem::CreateFromKnownIcon(L"Brush Size", RadialControllerMenuKnownIcon::InkThickness);

    g_menuItems.push_back(zoom);
    g_menuItems.push_back(brushSize);

    auto menu = g_controller.Menu();

    for (const auto& item : g_menuItems) {
        menu.Items().Append(item);
    }
    
    g_controller.RotationResolutionInDegrees(0.3);
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