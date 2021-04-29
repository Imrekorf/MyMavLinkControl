#include <iostream>
#include <WinSerial.h>
#include <chrono>

// based on: https://web.archive.org/web/20180127160838/http://bd.eduweb.hhs.nl/micprg/pdf/serial-win.pdf

using namespace WinSer;

std::string ErrorFormatter(){
	char lastError[1024];
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)lastError,
		1024,
		NULL);
	return std::string(lastError);
}

Serial::Serial(char* Port, SerParam SP, SerTimeOut ST){
	// setup handle
	hSerial = CreateFile(
		LPCWSTR(Port),					// specify com port, L is to assign string as LPCWSTR
		GENERIC_READ | GENERIC_WRITE,	// define if port is read, write or both
		0,								// should be 0
		0,								// should be 0
		OPEN_EXISTING,					// COM port already exists, thus open existing
		FILE_ATTRIBUTE_NORMAL,			// Dont do anything fancy with the file
		0								// should be 0
	);

	if( hSerial == INVALID_HANDLE_VALUE ){
		if(GetLastError() == ERROR_FILE_NOT_FOUND){
			throw SerialException("Serial Port: " + std::string(Port) + " does not exist");
		}
		throw SerialException(ErrorFormatter());
	}

	// setup parameters
	DCB dcbSerialParams = {0}; // set fields to 0
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);	// windows quirk go brrr

	if (!GetCommState(hSerial, &dcbSerialParams)){	// fill in parameters
		// error getting state
		throw SerialException(ErrorFormatter());
	}

	dcbSerialParams.BaudRate = SP.rate;		// set baudrate
	dcbSerialParams.ByteSize = SP.bytesize; // set bytesize
	dcbSerialParams.StopBits = SP.SB;		// set stopbit type
	dcbSerialParams.Parity 	 = SP.P;		// set parity

	if(!SetCommState(hSerial, &dcbSerialParams)){
		// error setting serial port state
		throw SerialException(ErrorFormatter());
	}

	// setup timeouts

	timeouts = {0};
	timeouts.ReadIntervalTimeout 		 = ST.ReadIntervalTimeout;
	timeouts.ReadTotalTimeoutMultiplier  = ST.ReadTotalTimeoutMultiplier;
	timeouts.ReadTotalTimeoutConstant 	 = ST.ReadTotalTimeoutConstant;
	timeouts.WriteTotalTimeoutConstant 	 = ST.WriteTotalTimeoutConstant;
	timeouts.WriteTotalTimeoutMultiplier = ST.WriteTotalTimeoutMultiplier;

	if(!SetCommTimeouts(hSerial, &timeouts)){
		// error occurred. Inform user
		throw SerialException(ErrorFormatter());
	}

	// start threads
	ReadThread = std::thread(ReadThreadFunc, std::ref(IncomingBuffer), std::ref(hSerial));
	SendThread = std::thread(SendThreadFunc, std::ref(OutGoingBuffer), std::ref(hSerial));
}

Serial::~Serial(){
	OutGoingBuffer.StopThread = true;
	IncomingBuffer.StopThread = true;

	ReadThread.join();
	SendThread.join();

	CloseHandle(hSerial);
}

unsigned int Serial::available(){
	return IncomingBuffer.count;
}

unsigned int Serial::availableForWrite(){
	return SERIALBUFFERSIZE - OutGoingBuffer.count;
}

//! TODO: wait for transmission of outgoing data to complete
void Serial::flush(unsigned int refreshrate){
	while(1){
		OutGoingBuffer.Mutex.lock();
		if(OutGoingBuffer.count == 0){
			break;
		}
		OutGoingBuffer.Mutex.unlock();
		// keep mutex unlocked for refreshrate
		std::this_thread::sleep_for(std::chrono::milliseconds(refreshrate));
	}
}

void Serial::clearBuffer(){
	IncomingBuffer.flushbuffer();
	OutGoingBuffer.flushbuffer();
}

void Serial::setTimeout(SerTimeOut ST){
	timeouts.ReadIntervalTimeout 		 = ST.ReadIntervalTimeout;
	timeouts.ReadTotalTimeoutMultiplier  = ST.ReadTotalTimeoutMultiplier;
	timeouts.ReadTotalTimeoutConstant 	 = ST.ReadTotalTimeoutConstant;
	timeouts.WriteTotalTimeoutConstant 	 = ST.WriteTotalTimeoutConstant;
	timeouts.WriteTotalTimeoutMultiplier = ST.WriteTotalTimeoutMultiplier;

	OutGoingBuffer.Mutex.lock();
	IncomingBuffer.Mutex.lock();

	if(!SetCommTimeouts(hSerial, &timeouts)){
		// error occurred. Inform user
		throw SerialException(ErrorFormatter());
	}

	OutGoingBuffer.Mutex.unlock();
	IncomingBuffer.Mutex.unlock();
}

int Serial::read(){
	return IncomingBuffer.pop();
}

int Serial::readBytes(char* buffer, unsigned int length){
	unsigned int i = 0;
	for(; i < length; i++){
		char temp = IncomingBuffer.pop();
		if(temp == -1){
			break;
		}
		buffer[i] = temp;
	}
	return i;
}

int Serial::readBytesUntil(char terminator, char* buffer, int length){
	unsigned int i = 0;
	for(; i < length; i++){
		char temp = IncomingBuffer.pop();
		if(temp == -1 || temp == terminator){
			break;
		}
		buffer[i] = temp;
	}
	return i;
}

std::string Serial::readString(){
	std::string S;
	char temp = IncomingBuffer.pop();
	while(temp != -1){
		S += temp;
		temp = IncomingBuffer.pop();
	}
	return S;
}

std::string Serial::readStringUntil(char terminator){
	std::string S;
	char temp = IncomingBuffer.pop();
	while(temp != -1 && temp != terminator){
		S += temp;
		temp = IncomingBuffer.pop();
	}
	return S;
}

void Serial::write(char val){
	try {
		OutGoingBuffer.push(val);
	}
	catch(std::exception e){
		e.what();
	}
}

void Serial::write(std::string str){
	for(char const &c : str)
	{
		write(c);
	}
}

void Serial::write(char* buf, unsigned int len){
	for(int i = 0; i < len; i++)
	{
		write(buf[i]);
	}
}

int Serial::ReadThreadFunc(Buffer::Buffer& IncomingBuffer, HANDLE& hSerial){
	while(!IncomingBuffer.StopThread){
		char szBuff[SERIALBYTESREADATONCE + 1] = {0};
		DWORD dwBytesRead = 0;
		if(!ReadFile(hSerial, szBuff, SERIALBYTESREADATONCE, &dwBytesRead, NULL)){
			// error occurred. Inform user
			std::cout << ErrorFormatter << std::endl;
			//throw SerialException(ErrorFormatter());
		}
		if(dwBytesRead > 0){
			IncomingBuffer.Mutex.lock();
			for(int i = 0; i < SERIALBYTESREADATONCE; i++){
				IncomingBuffer.push(szBuff[i]);
			}
			IncomingBuffer.Mutex.unlock();
		}
	}
}

int Serial::SendThreadFunc(Buffer::Buffer& OutGoingBuffer, HANDLE& hSerial){
	while(!OutGoingBuffer.StopThread){
		char szBuff[SERIALBYTESWRITEATONCE + 1] = {0};
		DWORD dwBytesRead = 0;

		if(OutGoingBuffer.count > 0){
			OutGoingBuffer.Mutex.lock();
			for(int i = 0; i < SERIALBYTESWRITEATONCE; i++){
				szBuff[i] = OutGoingBuffer.pop();
			}
			OutGoingBuffer.Mutex.unlock();
		}
		
		if(!WriteFile(hSerial, szBuff, SERIALBYTESWRITEATONCE, &dwBytesRead, NULL)){
			// error occurred. Inform user
			std::cout << ErrorFormatter << std::endl;
			//throw SerialException(ErrorFormatter());
		}
	}
}


#define n 25

void ReadSerial(HANDLE hSerial){
	char szBuff[n + 1] = {0};	// n bytes to be read in serial port
	DWORD dwBytesRead = 0;

	if(!ReadFile(hSerial, szBuff, n, &dwBytesRead, NULL)){
		// error occurred. Inform user
		ErrorFormatter();
	}
}

void WriteSerial(HANDLE hSerial){
	char szBuff[n + 1] = {0};
	DWORD dwBytesWritten = 0;

	if(!WriteFile(hSerial, szBuff, n, &dwBytesWritten, NULL)){
		// error occured. Inform user
		std::cout << ErrorFormatter << std::endl;
		//throw SerialException(ErrorFormatter());
	}
}