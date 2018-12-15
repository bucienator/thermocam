#include "pch.h"
#include "MainPage.h"

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Automation::Peers;
using namespace Windows::UI::Xaml::Media;

namespace winrt::viewer::implementation
{
    MainPage::MainPage()
    {
        InitializeComponent();
		NotifyUser(L"", NotifyType::StatusMessage);
    }

	void MainPage::NotifyUser(const std::wstring & strMessage, const NotifyType type)
	{
		// If called from the UI thread, then update immediately.
		// Otherwise, schedule a task on the UI thread to perform the update.
		if (Dispatcher().HasThreadAccess())
		{
			UpdateStatus(strMessage, type);
		}
		else
		{
			Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=]() {UpdateStatus(strMessage, type); });
		}
	}

	void MainPage::UpdateStatus(const std::wstring & strMessage, const NotifyType type)
	{
		switch (type)
		{
		case NotifyType::StatusMessage:
			StatusBorder().Background(SolidColorBrush(Colors::Green()));
			break;
		case NotifyType::ErrorMessage:
			StatusBorder().Background(SolidColorBrush(Colors::Red()));
			break;
		}

		StatusBlock().Text(strMessage);

		// Collapse the StatusBlock if it has no text to conserve real estate.
		if (StatusBlock().Text() != L"")
		{
			StatusBorder().Visibility(Visibility::Visible);
			StatusPanel().Visibility(Visibility::Visible);
		}
		else
		{
			StatusBorder().Visibility(Visibility::Collapsed);
			StatusPanel().Visibility(Visibility::Collapsed);
		}

		// Raise an event if necessary to enable a screen reader to announce the status update.
		const auto peer = FrameworkElementAutomationPeer::FromElement(StatusBlock());
		if (peer != nullptr)
		{
			peer.RaiseAutomationEvent(AutomationEvents::LiveRegionChanged);
		}
	}

}
