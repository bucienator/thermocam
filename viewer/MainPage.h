//
// Declaration of the MainPage class.
//

#pragma once

#include "MainPage.g.h"

using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Windows::System::Display;
using namespace Windows::System::Threading;
using namespace Windows::UI::Xaml::Media::Imaging;

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
		void OnAdvertisementReceived(BluetoothLEAdvertisementWatcher watcher, BluetoothLEAdvertisementReceivedEventArgs eventArgs);
		void OnAdvertisementStopped(BluetoothLEAdvertisementWatcher watcher, BluetoothLEAdvertisementWatcherStoppedEventArgs eventArgs);
		void OnBLEConnectionStatusChanged(BluetoothLEDevice device, IInspectable object);
		void OnBLEGattServicesChanged(BluetoothLEDevice device, IInspectable object);
		void OnBLENameChanged(BluetoothLEDevice device, IInspectable object);
		void OnThermocamImageUpdate(GattCharacteristic chr, GattValueChangedEventArgs eventArgs);
		void OnGrabImageTime(ThreadPoolTimer timer);

		void DisconnectBLE();
		void StopAdvWatcher();
		void StartAdvWatcher();
		void StartAdvWatcherIfNeeded();

	private:
		void UpdateStatus(const std::wstring & strMessage, NotifyType type);
		IAsyncAction SubscribeToThermocamImagesAsync(const uint64_t addr);
		void ProcessThermocamImageData(IBuffer data);

		bool seekConnection;
		BluetoothLEAdvertisementWatcher advWatcher;

		static const GUID thermocamServiceUUID;
		static const GUID thermocamCharacteristiccUUID;
		std::atomic<uint64_t> clientAddr; // address of the BLE client we are trying to connect to
		BluetoothLEDevice client;
		GattCharacteristic thermocamChr;
		SoftwareBitmapSource thermocamBitmap;

		ThreadPoolTimer imageGrabberTimestamp;
		std::mutex grabberLock;

		DisplayRequest displayRequest;
		std::atomic<uint32_t> requestCount;

		event_token tokenForConnectionStatusChanged;
		event_token tokenForGattServicesChanged;
		event_token tokenForNameChanged;
		event_token tokenForCharacteristicValueChanged;

		std::vector<uint32_t> colorScale;
		float min;
		float max;
	};
}

namespace winrt::viewer::factory_implementation
{
    struct MainPage : MainPageT<MainPage, implementation::MainPage>
    {
    };
}
