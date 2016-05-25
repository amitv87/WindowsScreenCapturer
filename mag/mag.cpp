// mag.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include <magnification.h>
#include <wincodec.h>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <ctime>
#include <chrono>
using std::ofstream;
using namespace std::chrono;
using namespace std;

long long getTS() {
	return duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count();
}

class ScreenCapturer {
private:
	bool running_;
	bool is_screencast_ = true;


	typedef BOOL(WINAPI* MagImageScalingCallback)(HWND hwnd,
		void* srcdata,
		MAGIMAGEHEADER srcheader,
		void* destdata,
		MAGIMAGEHEADER destheader,
		RECT unclipped,
		RECT clipped,
		HRGN dirty);
	typedef BOOL(WINAPI* MagInitializeFunc)(void);
	typedef BOOL(WINAPI* MagUninitializeFunc)(void);
	typedef BOOL(WINAPI* MagSetWindowSourceFunc)(HWND hwnd, RECT rect);
	typedef BOOL(WINAPI* MagSetWindowFilterListFunc)(HWND hwnd,
		DWORD dwFilterMode,
		int count,
		HWND* pHWND);
	typedef BOOL(WINAPI* MagSetImageScalingCallbackFunc)(
		HWND hwnd,
		MagImageScalingCallback callback);

	static BOOL WINAPI OnMagImageScalingCallback(HWND hwnd,
		void* srcdata,
		MAGIMAGEHEADER srcheader,
		void* destdata,
		MAGIMAGEHEADER destheader,
		RECT unclipped,
		RECT clipped,
		HRGN dirty);

	HWND excluded_window_;

	// Used to suppress duplicate logging of SetThreadExecutionState errors.
	bool set_thread_execution_state_failed_;

	// Used for getting the screen dpi.
	HDC desktop_dc_;

	HMODULE mag_lib_handle_;
	MagInitializeFunc mag_initialize_func_;
	MagUninitializeFunc mag_uninitialize_func_;
	MagSetWindowSourceFunc set_window_source_func_;
	MagSetWindowFilterListFunc set_window_filter_list_func_;
	MagSetImageScalingCallbackFunc set_image_scaling_callback_func_;

	// The hidden window hosting the magnifier control.
	HWND host_window_;
	// The magnifier control that captures the screen.
	HWND magnifier_window_;

	// True if the magnifier control has been successfully initialized.
	bool magnifier_initialized_;

	// True if the last OnMagImageScalingCallback was called and handled
	// successfully. Reset at the beginning of each CaptureImage call.
	bool magnifier_capture_succeeded_;

	bool InitializeMagnifier();

public:
	ScreenCapturer(const float width, const float height, const int fps);
	~ScreenCapturer() {}
	void Start();
	void Stop();
	void SetScreencast(bool is_screencast) { is_screencast_ = is_screencast; }
	bool IsScreencast() { return is_screencast_; }
	bool IsRunning() { return running_; }
	bool CaptureImage();
	static BOOL OnMagImageScalingCallback();
};

const int FRAME_RATE_30 = 30;

int main(int argc, char* argv[])
{
	int index = 0, cur_index = 0;
	long long ts1 = 0, ts2 = 0;
	int targetFramInterval = 1000 / (FRAME_RATE_30 + 0.5);

	if (argc > 1) {
		std::cout << "target frame rate: " << argv[1] << std::endl;
		float targetFrameRate = atof(argv[1]);
		if (targetFrameRate > 0)
			targetFramInterval = 1000 / (targetFrameRate + 0.5);
	}

	bool save_img = false;
	if (argc > 2) {
		save_img = true;
		std::cout << "save image enabled: " << std::endl;
		system("mkdir img_mag");
		system("del img_mag\\*.argb");
	}
	float elapsedTime = 0, cur_elapsedTime = 0;

	ScreenCapturer *sc = new ScreenCapturer(1440, 900, 30);
	sc->Start();
	while (true) {
		ts1 = getTS();

		if (cur_index > 0) {
			float cur_frameRate = cur_index * 1000 / cur_elapsedTime;
			float avg_frameRate = index * 1000 / elapsedTime;
			std::cout << "\r" << "avg_frameRate: " << avg_frameRate << ", cur_frameRate: " << cur_frameRate << std::flush;
		}
		sc->CaptureImage();

		index += 1;
		cur_index += 1;
		ts2 = getTS();
		int sleep_duration = ts2 - ts1;
		sleep_duration = sleep_duration > targetFramInterval ? 0 : targetFramInterval - sleep_duration;
		Sleep(sleep_duration);

		elapsedTime += getTS() - ts1;
		cur_elapsedTime += getTS() - ts1;
		if (cur_elapsedTime > 1000) {
			cur_elapsedTime = 0;
			cur_index = 0;
		}
	}
	return 0;
}


static LPCTSTR kMagnifierHostClass = L"ScreenCapturerWinMagnifierHost";
static LPCTSTR kHostWindowName = L"MagnifierHost";
static LPCTSTR kMagnifierWindowClass = L"Magnifier";
static LPCTSTR kMagnifierWindowName = L"MagnifierWindow";

ScreenCapturer::ScreenCapturer(const float width, const float height, const int fps){
	InitializeMagnifier();
};

void ScreenCapturer::Start() {
	running_ = true;
}

void ScreenCapturer::Stop() {
	running_ = false;
}

bool ScreenCapturer::InitializeMagnifier() {
	assert(!magnifier_initialized_);

	desktop_dc_ = GetDC(NULL);

	mag_lib_handle_ = LoadLibrary(L"Magnification.dll");
	if (!mag_lib_handle_)
		return false;

	// Initialize Magnification API function pointers.
	mag_initialize_func_ = reinterpret_cast<MagInitializeFunc>(
		GetProcAddress(mag_lib_handle_, "MagInitialize"));
	mag_uninitialize_func_ = reinterpret_cast<MagUninitializeFunc>(
		GetProcAddress(mag_lib_handle_, "MagUninitialize"));
	set_window_source_func_ = reinterpret_cast<MagSetWindowSourceFunc>(
		GetProcAddress(mag_lib_handle_, "MagSetWindowSource"));
	set_window_filter_list_func_ = reinterpret_cast<MagSetWindowFilterListFunc>(
		GetProcAddress(mag_lib_handle_, "MagSetWindowFilterList"));
	set_image_scaling_callback_func_ =
		reinterpret_cast<MagSetImageScalingCallbackFunc>(
			GetProcAddress(mag_lib_handle_, "MagSetImageScalingCallback"));

	if (!mag_initialize_func_ || !mag_uninitialize_func_ ||
		!set_window_source_func_ || !set_window_filter_list_func_ ||
		!set_image_scaling_callback_func_) {
		std::cout << "Failed to initialize ScreenCapturerWinMagnifier: "
			<< "library functions missing." << std::endl;
		return false;
	}

	BOOL result = mag_initialize_func_();
	if (!result) {
		std::cout << "Failed to initialize ScreenCapturerWinMagnifier: "
			<< "error from MagInitialize " << GetLastError() << std::endl;;
		return false;
	}

	HMODULE hInstance = NULL;
	result = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<char*>(&DefWindowProc),
		&hInstance);
	if (!result) {
		mag_uninitialize_func_();
		std::cout << "Failed to initialize ScreenCapturerWinMagnifier: "
			<< "error from GetModulehandleExA " << GetLastError() << std::endl;
		return false;
	}

	// Register the host window class. See the MSDN documentation of the
	// Magnification API for more infomation.
	WNDCLASSEX wcex = {};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.lpfnWndProc = &DefWindowProc;
	wcex.hInstance = hInstance;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.lpszClassName = kMagnifierHostClass;

	// Ignore the error which may happen when the class is already registered.
	RegisterClassEx(&wcex);

	// Create the host window.
	host_window_ = CreateWindowEx(WS_EX_LAYERED,
		kMagnifierHostClass,
		kHostWindowName,
		0,
		0, 0, 0, 0,
		NULL,
		NULL,
		hInstance,
		NULL);
	if (!host_window_) {
		mag_uninitialize_func_();
		std::cout << "Failed to initialize ScreenCapturerWinMagnifier: "
			<< "error from creating host window " << GetLastError() << std::endl;
		return false;
	}

	// Create the magnifier control.
	magnifier_window_ = CreateWindow(kMagnifierWindowClass,
		kMagnifierWindowName,
		WS_CHILD | WS_VISIBLE,
		0, 0, 0, 0,
		host_window_,
		NULL,
		hInstance,
		NULL);
	if (!magnifier_window_) {
		mag_uninitialize_func_();
		std::cout << "Failed to initialize ScreenCapturerWinMagnifier: "
			<< "error from creating magnifier window "
			<< GetLastError() << std::endl;
		return false;
	}

	// Hide the host window.
	ShowWindow(host_window_, SW_HIDE);

	// Set the scaling callback to receive captured image.
	result = set_image_scaling_callback_func_(
		magnifier_window_,
		&ScreenCapturer::OnMagImageScalingCallback);
	if (!result) {
		mag_uninitialize_func_();
		std::cout << "Failed to initialize ScreenCapturerWinMagnifier: "
			<< "error from MagSetImageScalingCallback "
			<< GetLastError() << std::endl;
		return false;
	}

	if (excluded_window_) {
		result = set_window_filter_list_func_(
			magnifier_window_, MW_FILTERMODE_EXCLUDE, 1, &excluded_window_);
		if (!result) {
			mag_uninitialize_func_();
			std::cout << "Failed to initialize ScreenCapturerWinMagnifier: "
				<< "error from MagSetWindowFilterList "
				<< GetLastError() << std::endl;
			return false;
		}
	}

	magnifier_initialized_ = true;
	return true;
}
int index = 0;
BOOL ScreenCapturer::OnMagImageScalingCallback(
	HWND hwnd,
	void* srcdata,
	MAGIMAGEHEADER srcheader,
	void* destdata,
	MAGIMAGEHEADER destheader,
	RECT unclipped,
	RECT clipped,
	HRGN dirty) {

	//sc->CaptureFrame((char*)srcdata, srcheader.width, srcheader.height, srcheader.width * srcheader.height * 4);

	//std::cout << "frame cpatured, width: " << srcheader.width;
	//std::cout << " height: " << srcheader.height << std::endl;
	//srcheader.height, srcheader.width * srcheader.height * 4

	//if (save_img) {
	int size = srcheader.width * srcheader.height * 4;
	if (true) {
		char file[100] = "";
		sprintf_s(file, "img_mag\\raw%d.argb", index++);
		ofstream fout;
		fout.open(file, std::ios::binary);
		fout.write((char*)srcdata, size);
		fout.close();
	}

	return TRUE;
}

bool ScreenCapturer::CaptureImage() {
	assert(magnifier_initialized_);

	int x = GetSystemMetrics(SM_CXSCREEN), y = GetSystemMetrics(SM_CYSCREEN);

	// Set the magnifier control to cover the captured rect. The content of the
	// magnifier control will be the captured image.
	//std::cout << "CaptureImage 1" << std::endl;
	BOOL result = SetWindowPos(magnifier_window_,
		NULL,
		0, 0, x, y,
		0);
	//std::cout << "CaptureImage 2" << std::endl;
	if (!result) {
		std::cout << "Failed to call SetWindowPos: " << GetLastError() << std::endl;
		return false;
	}
	//std::cout << "CaptureImage 3" << std::endl;

	magnifier_capture_succeeded_ = false;

	RECT native_rect = {0,0,x,y};

	//std::cout << "CaptureImage 4" << std::endl;
	// OnCaptured will be called via OnMagImageScalingCallback and fill in the
	// frame before set_window_source_func_ returns.
	result = set_window_source_func_(magnifier_window_, native_rect);

	//std::cout << "CaptureImage 5" << std::endl;
	if (!result) {
		std::cout << "Failed to call MagSetWindowSource: " << GetLastError() << std::endl;
		return false;
	}

	return magnifier_capture_succeeded_;
}
