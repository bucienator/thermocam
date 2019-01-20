#pragma once
#include "App.xaml.g.h"

namespace winrt::viewer::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs const&);
        void OnEnteredBackground(IInspectable const&, Windows::ApplicationModel::EnteredBackgroundEventArgs const&);
		void OnLeavingBackground(IInspectable const&, Windows::ApplicationModel::LeavingBackgroundEventArgs const&);
        void OnNavigationFailed(IInspectable const&, Windows::UI::Xaml::Navigation::NavigationFailedEventArgs const&);
    };
}
