#include "pch.h"
#include "MainPage.h"

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Automation::Peers;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::Storage::Streams;

namespace winrt::viewer::implementation
{

	MainPage::MainPage() : client{ nullptr }, thermocamChr{ nullptr }
    {
        InitializeComponent();
		NotifyUser(L"", NotifyType::StatusMessage);
		clientAddr = 0;
		advWatcher.Received({ this, &MainPage::OnAdvertisementReceived });
		advWatcher.Stopped({ this, &MainPage::OnAdvertisementStopped });
		advWatcher.Start();
    }

	const GUID MainPage::thermocamServiceUUID = { 0x97B8FCA2, 0x45A8, 0x478C, 0x9E, 0x85, 0xCC, 0x85, 0x2A, 0xF2, 0xE9, 0x50 };
	const GUID MainPage::thermocamCharacteristiccUUID = { 0x52e66cfc, 0x9dd2, 0x4932, 0x8e, 0x81, 0x7e, 0xaf, 0x2c, 0x6e, 0x2c, 0x53 };

	void MainPage::OnBLEConnectionStatusChanged(BluetoothLEDevice device, IInspectable object)
	{
		if (device != client) {
			return;
		}
		switch (device.ConnectionStatus()) {
		case BluetoothConnectionStatus::Connected:
			NotifyUser(L"Client connected.", NotifyType::ErrorMessage);
			break;
		case BluetoothConnectionStatus::Disconnected:
			NotifyUser(L"Client disconnected.", NotifyType::ErrorMessage);
			break;
		}
	}

	void MainPage::OnBLEGattServicesChanged(BluetoothLEDevice device, IInspectable object)
	{
		NotifyUser(L"GATT services changed.", NotifyType::ErrorMessage);
	}

	void MainPage::OnBLENameChanged(BluetoothLEDevice device, IInspectable object)
	{
		NotifyUser(L"BLE Name changed.", NotifyType::ErrorMessage);
	}

	void MainPage::OnThermoImageUpdate(GattCharacteristic chr, GattValueChangedEventArgs eventArgs)
	{
		static int cnt = 0;

		DataReader reader = DataReader::FromBuffer(eventArgs.CharacteristicValue());
		std::wstring log = std::wstring(L"Received data update ") + std::to_wstring(cnt++) + L", data size: " + std::to_wstring(reader.UnconsumedBufferLength());
		NotifyUser(log, NotifyType::StatusMessage);
		std::vector<uint8_t> data(128, 0);
		reader.ReadBytes(data);
	}

	IAsyncAction MainPage::SubscribeToThermocamImagesAsync(const uint64_t addr)
	{
		client = co_await BluetoothLEDevice::FromBluetoothAddressAsync(addr);
		client.ConnectionStatusChanged({ this, &MainPage::OnBLEConnectionStatusChanged });
		client.GattServicesChanged({ this, &MainPage::OnBLEGattServicesChanged });
		client.NameChanged({ this, &MainPage::OnBLENameChanged });
		
		auto tcServicesResult = co_await client.GetGattServicesForUuidAsync(thermocamServiceUUID, BluetoothCacheMode::Uncached);
		if (tcServicesResult.Status() != GattCommunicationStatus::Success) {
			// failed to query services
			co_return;
		}
		auto thermocamServices = tcServicesResult.Services();
		if(thermocamServices.Size() == 0) {
			// some error, reset connection
			co_return;
		}
		if (thermocamServices.Size() > 1) {
			// more then one service? That is weird
			NotifyUser(L"More than one service returned for thermocam uuid.", NotifyType::ErrorMessage);
		}

		GattDeviceService thermoceSvc = thermocamServices.GetAt(0);
		auto tcCharacteristicsResult = co_await thermoceSvc.GetCharacteristicsForUuidAsync(thermocamCharacteristiccUUID, BluetoothCacheMode::Uncached);
		if (tcCharacteristicsResult.Status() != GattCommunicationStatus::Success) {
			// some error
			co_return;
		}

		auto thermocamCharacteristics = tcCharacteristicsResult.Characteristics();
		if (thermocamCharacteristics.Size() == 0) {
			// some error, reset connection
			co_return;
		}
		if (thermocamCharacteristics.Size() > 1) {
			// more then one service? That is weird
			NotifyUser(L"More than one characteristics returned for thermocam uuid.", NotifyType::ErrorMessage);
		}

		thermocamChr = thermocamCharacteristics.GetAt(0);

		thermocamChr.ValueChanged({ this, &MainPage::OnThermoImageUpdate });
		GattCommunicationStatus status = co_await thermocamChr.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::Notify);
		if (status != GattCommunicationStatus::Success) {
			// some error, drop connection
			co_return;
		}

	}

	void MainPage::OnAdvertisementReceived(BluetoothLEAdvertisementWatcher watcher, BluetoothLEAdvertisementReceivedEventArgs eventArgs)
	{
		
		
		auto uuids = eventArgs.Advertisement().ServiceUuids();
		for (auto uuid : uuids) {
			if (uuid == thermocamServiceUUID) {
				NotifyUser(L"Found thermocam service on a device. Connecting there.", NotifyType::StatusMessage);

				// this callback might have been called in parallel on multiple threads,
				// so we have to make sure to start connecting to a single client
				uint64_t expectedAddr = 0;
				if (clientAddr.compare_exchange_weak(expectedAddr, eventArgs.BluetoothAddress())) {
					// we enter this block only if we successfully replaced a value of zero to the currently found device's addr.
					advWatcher.Stop();
					SubscribeToThermocamImagesAsync(eventArgs.BluetoothAddress());
				}
			}
		}
	}



	void MainPage::OnAdvertisementStopped(BluetoothLEAdvertisementWatcher watcher, BluetoothLEAdvertisementWatcherStoppedEventArgs eventArgs)
	{
		NotifyUser(L"Advertisement watcher stopped", NotifyType::StatusMessage);
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
