#include "pch.h"
#include "MainPage.h"
#include "resample.h"

using namespace winrt;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Storage::Streams;
using namespace Windows::UI;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Automation::Peers;
using namespace Windows::UI::Xaml::Media;

namespace winrt::viewer::implementation
{

	std::vector<uint32_t> GenerateIronScale()
	{
		std::vector<uint32_t> colorScale;
		colorScale.resize(256);
		/*
			0		0.0  -> (0, 0, 128)    (dark blue)
			64		0.25 -> (0, 255, 0)    (green)
			128		0.5  -> (255, 255, 0)  (yellow)
			192		0.75 -> (255, 128, 0)  (orange)
			255		1.0  -> (255, 0, 0)    (red)
		*/
		const uint32_t alfa = 255;
		for (unsigned i = 0; i < 64; ++i) {
			const float a = 1 - i / 64.0;
			const float b = i / 64.0;
			const uint32_t red = 0;
			const uint32_t green = 255 * b;
			const uint32_t blue = 128 * a;
			colorScale[i] = (alfa << 24) | (red << 16) | (green << 8) | (blue);
		}
		for (unsigned i = 64; i < 128; ++i) {
			const float a = 1 - (i - 64) / 64.0;
			const float b = (i - 64) / 64.0;
			const uint32_t red = 255 * b;
			const uint32_t green = 255;
			const uint32_t blue = 0;
			colorScale[i] = (alfa << 24) | (red << 16) | (green << 8) | (blue);
		}
		for (unsigned i = 128; i < 192; ++i) {
			const float a = 1 - (i - 128) / 64.0;
			const float b = (i - 128) / 64.0;
			const uint32_t red = 255;
			const uint32_t green = 255 * a + 128 * b;
			const uint32_t blue = 0;
			colorScale[i] = (alfa << 24) | (red << 16) | (green << 8) | (blue);
		}
		for (unsigned i = 192; i < 256; ++i) {
			const float a = 1 - (i - 192) / 64.0;
			const float b = (i - 192) / 64.0;
			const uint32_t red = 255;
			const uint32_t green = 128 * a;
			const uint32_t blue = 0;
			colorScale[i] = (alfa << 24) | (red << 16) | (green << 8) | (blue);
		}

		return colorScale;
	}

	MainPage::MainPage() : client{ nullptr }, thermocamChr{ nullptr }, min{ 20 }, max{ 30 }, seekConnection(false)
    {
        InitializeComponent();
		NotifyUser(L"", NotifyType::StatusMessage);
		clientAddr = 0;
		advWatcher.Received({ this, &MainPage::OnAdvertisementReceived });
		advWatcher.Stopped({ this, &MainPage::OnAdvertisementStopped });

		thermalImage().Source(thermocamBitmap);

		colorScale = GenerateIronScale();
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
			DisconnectBLE();
			StartAdvWatcherIfNeeded();
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
		std::vector<uint8_t> data(128, 0);
		reader.ReadBytes(data);

		std::vector<float> temperatures(64, 0.0f);
		for (unsigned i = 0; i < 64; ++i) {
			const uint16_t pixel = data[i * 2] | (data[i * 2 + 1] << 8);
			const bool sign = pixel & 0x0400;
			if (!sign) {
				// positive
				temperatures[i] = static_cast<float>(pixel & 0x07ff) / 4;
			}
			else {
				temperatures[i] = 0;// static_cast<float>(pixel & 0x07ff) / 4;
			}
		}

		const int scaled_size = 100; // resample image to 100x100
		temperatures = resampleThermalImage(temperatures, scaled_size);

		const auto minmax = std::minmax_element(temperatures.begin(), temperatures.end());

		if (*minmax.first < min) {
			min = *minmax.first;
		}
		if (*minmax.second > max) {
			max = *minmax.second;
		}
		if (max == min) {
			max = min + 0.25;
		}

		std::wstring log = std::wstring(L"Min: ") + std::to_wstring(*minmax.first) + L"(" + std::to_wstring(min) + L") max: " + std::to_wstring(*minmax.second) + L"(" + std::to_wstring(max) + L")";
		NotifyUser(log, NotifyType::StatusMessage);

		SoftwareBitmap sb(BitmapPixelFormat::Bgra8, scaled_size, scaled_size, BitmapAlphaMode::Premultiplied);
		{
			auto buffer = sb.LockBuffer(BitmapBufferAccessMode::Write);
			auto reference = buffer.CreateReference();
			auto interop = reference.as<IMemoryBufferByteAccess>();
			uint8_t * data;
			uint32_t length;
			interop->GetBuffer(&data, &length);
			uint32_t * pixels = reinterpret_cast<uint32_t *>(data);

			for (unsigned i = 0; i < scaled_size * scaled_size; ++i) {
				const uint8_t pixel = static_cast<uint8_t>((temperatures[i] - min) / (max - min) * 255);
				pixels[i] = colorScale[pixel];
			}
		}

		Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [sb, this]() {
			thermocamBitmap.SetBitmapAsync(sb);
		});
	}

	void MainPage::DisconnectBLE()
	{
		uint32_t expected = 1;
		if (requestCount.compare_exchange_strong(expected, 0)) {
			displayRequest.RequestRelease();
		}

		if (thermocamChr) {
			if (tokenForCharacteristicValueChanged) {
				thermocamChr.ValueChanged(tokenForCharacteristicValueChanged);
			}
			thermocamChr = nullptr;
		}
		if (client) {
			client.ConnectionStatusChanged(tokenForConnectionStatusChanged);
			client.GattServicesChanged(tokenForGattServicesChanged);
			client.NameChanged(tokenForNameChanged);
			client = nullptr;
		}
		clientAddr.exchange(0);
	}
	void MainPage::StopAdvWatcher()
	{
		advWatcher.Stop();
		seekConnection = false;
	}
	void MainPage::StartAdvWatcher()
	{
		if (clientAddr == 0) {
			// only start the asvertisement watcher, if we don't have a live connection
			advWatcher.Start();
		}
		seekConnection = true;
	}
	void MainPage::StartAdvWatcherIfNeeded()
	{
		if (seekConnection) {
			StartAdvWatcher();
		}
	}

	IAsyncAction MainPage::SubscribeToThermocamImagesAsync(const uint64_t addr)
	{
		client = co_await BluetoothLEDevice::FromBluetoothAddressAsync(addr);
		tokenForConnectionStatusChanged = client.ConnectionStatusChanged({ this, &MainPage::OnBLEConnectionStatusChanged });
		tokenForGattServicesChanged = client.GattServicesChanged({ this, &MainPage::OnBLEGattServicesChanged });
		tokenForNameChanged = client.NameChanged({ this, &MainPage::OnBLENameChanged });
		
		auto tcServicesResult = co_await client.GetGattServicesForUuidAsync(thermocamServiceUUID, BluetoothCacheMode::Uncached);
		if (tcServicesResult.Status() != GattCommunicationStatus::Success) {
			// failed to query services
			DisconnectBLE();
			// TODO: add addr to blacklist
			StartAdvWatcherIfNeeded();
			co_return;
		}
		auto thermocamServices = tcServicesResult.Services();
		if(thermocamServices.Size() == 0) {
			// failed to query services
			DisconnectBLE();
			// TODO: add addr to blacklist
			StartAdvWatcherIfNeeded();
			co_return;
		}
		if (thermocamServices.Size() > 1) {
			// more than one service? That is weird
			NotifyUser(L"More than one service returned for thermocam uuid.", NotifyType::ErrorMessage);
		}

		GattDeviceService thermoceSvc = thermocamServices.GetAt(0);
		auto tcCharacteristicsResult = co_await thermoceSvc.GetCharacteristicsForUuidAsync(thermocamCharacteristiccUUID, BluetoothCacheMode::Uncached);
		if (tcCharacteristicsResult.Status() != GattCommunicationStatus::Success) {
			// failed to read characteristics for service
			DisconnectBLE();
			// TODO: add addr to blacklist
			StartAdvWatcherIfNeeded();
			co_return;
		}

		auto thermocamCharacteristics = tcCharacteristicsResult.Characteristics();
		if (thermocamCharacteristics.Size() == 0) {
			// failed to read characteristics for service
			DisconnectBLE();
			// TODO: add addr to blacklist
			StartAdvWatcherIfNeeded();
			co_return;
		}
		if (thermocamCharacteristics.Size() > 1) {
			// more than one characteristic? That is weird
			NotifyUser(L"More than one characteristics returned for thermocam uuid.", NotifyType::ErrorMessage);
		}

		thermocamChr = thermocamCharacteristics.GetAt(0);

		tokenForCharacteristicValueChanged = thermocamChr.ValueChanged({ this, &MainPage::OnThermoImageUpdate });
		GattCommunicationStatus status = co_await thermocamChr.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::Notify);
		if (status != GattCommunicationStatus::Success) {
			// failed to subscribe to notifications
			DisconnectBLE();
			// TODO: add addr to blacklist
			StartAdvWatcherIfNeeded();
			co_return;
		}

		uint32_t expected = 0;
		if (requestCount.compare_exchange_strong(expected, 1)) {
			displayRequest.RequestActive();
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
