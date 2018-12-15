//
// Declaration of the MainPage class.
//

#pragma once

#include "MainPage.g.h"

namespace winrt::viewer::implementation
{
	enum class NotifyType
	{
		StatusMessage,
		ErrorMessage,
	};


    struct MainPage : MainPageT<MainPage>
    {
        MainPage();

		void NotifyUser(const std::wstring & strMessage, NotifyType type);


        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);

	private:
		void UpdateStatus(const std::wstring & strMessage, NotifyType type);
    };
}

namespace winrt::viewer::factory_implementation
{
    struct MainPage : MainPageT<MainPage, implementation::MainPage>
    {
    };
}
