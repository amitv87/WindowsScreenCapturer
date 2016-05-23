// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <ctime>
#include <chrono>

using std::ofstream;
using namespace std::chrono;

typedef struct {
	HWND window;
	HDC windowDC;
	HDC memoryDC;
	HBITMAP bitmap;
	BITMAPINFOHEADER bitmapInfo;

	int width;
	int height;

	void *pixels;
} grabber_t;

grabber_t *grabber_create(HWND window);
void grabber_destroy(grabber_t *self);
void *grabber_grab(grabber_t *self);

long long getTS(){
	return duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count();
}

int main(int argc, char* argv[])
{
	int size = 0;
	byte *bytes = new byte[size];

	int index = 0, cur_index = 0;
	HRESULT hr;

	long long ts1 = 0, ts2 = 0;
	int targetFramInterval = 1000 / 30;

	if (argc > 1){
		std::cout << "target frame rate: " << argv[1] << std::endl;
		int targetFrameRate = atoi(argv[1]);
		if (targetFrameRate > 0)
			targetFramInterval = 1000 / targetFrameRate;
	}

	bool save_img = false;
	if (argc > 2){
		save_img = true;
		std::cout << "save image enabled: " << std::endl;
		system("mkdir img_gdi");
		system("del img_gdi\\*.argb");
	}

	float elapsedTime = 0, cur_elapsedTime = 0;

	grabber_t *source = grabber_create(GetDesktopWindow());

	while (true)
	{
		ts1 = getTS();

		if (cur_index > 0){
			float cur_frameRate = cur_index * 1000 / cur_elapsedTime;
			float avg_frameRate = index * 1000 / elapsedTime;
			std::cout << "\r" << "avg_frameRate: " << avg_frameRate << ", cur_frameRate: " << cur_frameRate << std::flush;
		}

		
		int tsize = source->bitmapInfo.biSizeImage;
		if (tsize != size) {
			size = tsize;
			if (bytes != nullptr)
				delete[] bytes;
			bytes = new byte[size];
		}

		//std::wcout << "size: " << size << ", ts: " << getTS().count() << std::endl;

		memcpy(bytes, grabber_grab(source), size);

		if (save_img){
			char file[100] = "";
			sprintf_s(file, "img_gdi\\raw%d.argb", index);
			ofstream fout;
			fout.open(file, std::ios::binary);
			fout.write((char*)bytes, size);
			fout.close();
		}

		index += 1;
		cur_index += 1;
		ts2 = getTS();
		int sleep_duration = ts2 - ts1;
		sleep_duration = sleep_duration > targetFramInterval ? 0 : targetFramInterval - sleep_duration;
		//std::cout << "sleeping for: " << sleep_duration << std::endl;
		Sleep(sleep_duration);

		elapsedTime += getTS() - ts1;
		cur_elapsedTime += getTS() - ts1;
		if (cur_elapsedTime > 1000){
			cur_elapsedTime = 0;
			cur_index = 0;
		}
	}
	grabber_destroy(source);
	return 0;
}


grabber_t *grabber_create(HWND window) {
	grabber_t *self = (grabber_t *)malloc(sizeof(grabber_t));
	memset(self, 0, sizeof(grabber_t));

	RECT rect;
	GetClientRect(window, &rect);

	self->window = window;

	self->width = rect.right - rect.left;
	self->height = rect.bottom - rect.top;

	self->windowDC = GetDC(self->window);
	self->memoryDC = CreateCompatibleDC(self->windowDC);
	self->bitmap = CreateCompatibleBitmap(self->windowDC, self->width, self->height);
	SetStretchBltMode(self->windowDC, HALFTONE);
	self->bitmapInfo.biSize = sizeof(BITMAPINFOHEADER);
	self->bitmapInfo.biPlanes = 1;
	self->bitmapInfo.biBitCount = 32;
	self->bitmapInfo.biWidth = self->width;
	self->bitmapInfo.biHeight = -self->height;
	self->bitmapInfo.biCompression = BI_RGB;
	self->bitmapInfo.biSizeImage = self->width * self->height * 4;

	self->pixels = malloc(self->bitmapInfo.biSizeImage);
	return self;
}

void grabber_destroy(grabber_t *self) {
	if (self == NULL) { return; }
	ReleaseDC(self->window, self->windowDC);
	DeleteDC(self->memoryDC);
	DeleteObject(self->bitmap);
	free(self->pixels);
	free(self);
}

void *grabber_grab(grabber_t *self) {
	if (self == NULL) { return NULL; }
	// uint32_t t1 = rtc::Time();
	// uint32_t t2;
	// t2 = rtc::Time();
	// LOG(LS_WARNING) << "t1: " << t2 - t1;
	// t1 = t2;
	HGDIOBJ hbmOld = SelectObject(self->memoryDC, self->bitmap);
	BitBlt(self->memoryDC, 0, 0, self->width, self->height, self->windowDC, 0, 0, SRCCOPY | CAPTUREBLT);
	GetDIBits(self->memoryDC, self->bitmap, 0, self->height, self->pixels, (BITMAPINFO*)&(self->bitmapInfo), DIB_RGB_COLORS);
	SelectObject(self->memoryDC, hbmOld);
	return self->pixels;
}
