#pragma once

#include <windows.h>
#include <thread>
#include <mutex>
#include <string>
#include <atomic>

#define SERIALBUFFERSIZE 1024
#define SERIALBYTESREADATONCE 1
#define SERIALBYTESWRITEATONCE 1

namespace WinSer {
	namespace Buffer {
		class SerialBufferException: public std::exception {
		public:
			enum class EType{
				Overflow,
				Underflow,
				WriteRetryTimeout
			};
		private:
			std::string message_;
			EType error;
		public:
			
			explicit SerialBufferException(const std::string& message, const EType E) : message_(message), error(E) {}
			const char* what() const noexcept override {
				return message_.c_str();
			}
			EType GetType(){
				return error;
			}
		};

		// Buffer logic
		typedef struct __buffer {
			unsigned int	count;
			char 		 	buff[SERIALBUFFERSIZE];
			unsigned int  	front	: 10;
			std::atomic<bool>	StopThread;
			std::mutex	  	Mutex;
			__buffer() : count(0), front(0), StopThread(false) {}
			void push(char C){
				// check if buffer is full
				Mutex.lock();
				if(count >= SERIALBUFFERSIZE){ throw SerialBufferException("Buffer overflow during push operation", SerialBufferException::EType::Overflow); return; }
				buff[count + front] = C;
				count++;
				Mutex.unlock();
			}
			char pop(void){
				Mutex.lock();
				if(!count){ throw SerialBufferException("Buffer empty during pop operation", SerialBufferException::EType::Underflow); return 0;}
				front++;
				count--;
				return buff[front-1];
				Mutex.unlock();
			}
			void flushbuffer(){
				Mutex.lock();
				count = 0;
				front = 0;
				Mutex.unlock();
			}
			unsigned int getbuffersize(){
				unsigned int size = 0;
				Mutex.lock();
				size = count;
				Mutex.unlock();
				return size;
			}
		} Buffer;
	}; // end of namespace Buffer


	class SerialException: public std::exception {
	private:
		std::string message_;

	public:
		explicit SerialException(const std::string& message) : message_(message) {}
		const char* what() const noexcept override {
			return message_.c_str();
		}
	};

	// Serial parameters
	struct SerParam {
		enum Baudrate {
			B110    = CBR_110,
			B300    = CBR_300,
			B600    = CBR_600,
			B1200   = CBR_1200,
			B2400   = CBR_2400,
			B4800   = CBR_4800,
			B9600   = CBR_9600,
			B14400  = CBR_14400,
			B19200  = CBR_19200,
			B38400  = CBR_38400,
			B57600  = CBR_57600,
			B115200 = CBR_115200,
			B128000 = CBR_128000,
			B256000 = CBR_256000,
		};

		enum StopBits {
			ONE = ONESTOPBIT,
			ONE5 = ONE5STOPBITS,
			TWO = TWOSTOPBITS
		};

		enum Parity {
			EVEN  = EVENPARITY,
			MARK  = MARKPARITY,
			NONE  = NOPARITY,
			ODD   = ODDPARITY,
			SPACE = SPACEPARITY
		};

		Baudrate rate 			= B9600;
		Parity P 				= Parity::NONE;
		StopBits SB				= StopBits::ONE;
		unsigned int bytesize 	= 8;
		SerParam(Baudrate rate = B9600, Parity P = Parity::NONE, StopBits SB = StopBits::ONE, unsigned int bytesize = 8) :
			rate(rate), P(P), SB(SB), bytesize(bytesize) {}
	};

	struct SerTimeOut {
		/* calculated with:
		* ReadIntervalTimeout = max time between bytes
		* ReadTotalTimeoutMultiplier = (MaxTimeOut) / (MaxBytesRead)
		* ReadTotalTimeoutConstant = (MaxTimeOut) - ReadTotalTimeoutMultiplier
		*/
		unsigned int ReadIntervalTimeout 		= 50;	// How long in milliseconds to wait between receiving characters before timing out
		unsigned int ReadTotalTimeoutMultiplier = 10;	// How much additional time to wait in milliseconds before returning for each byte that was requestd in the read operation
		unsigned int ReadTotalTimeoutConstant 	= 50;	// How long to wait in milliseconds before returning for read operation
		unsigned int WriteTotalTimeoutConstant	= 50;	// How long to wait in milliseconds before returning for write operation
		unsigned int WriteTotalTimeoutMultiplier= 10;	// How much additional time to wait in milliseconds before returning for each byte that was requestd in the write operation
		SerTimeOut() {}
	};

	class Serial {
	private:
		HANDLE hSerial;
		COMMTIMEOUTS timeouts;

		Buffer::Buffer IncomingBuffer;
		Buffer::Buffer OutGoingBuffer;
		
		
		static int ReadThreadFunc(Buffer::Buffer& IncomingBuffer, HANDLE& hSerial);
		static int SendThreadFunc(Buffer::Buffer& OutGoingBuffer, HANDLE& hSerial);

		std::thread ReadThread;
		std::thread SendThread;

	public:
		Serial(char* Port, SerParam SP = SerParam(), SerTimeOut ST = SerTimeOut());
		~Serial();
		
		//* returns the number of bytes available to read
		unsigned int available();

		//* returns the number of bytes available to write without blocking the write operation
		unsigned int availableForWrite();

		//* waits for transmission of outgoing serial data to complete
		//* refreshrate in milliseconds for internal timer for how quickly to check if the outgoing buffer is empty
		void flush(unsigned int refreshrate = 5);

		//* Clears currently stored buffer data, Incoming and Outgoing
		void clearBuffer();

		//* Sets the timeout parameters of the serial port
		void setTimeout(SerTimeOut ST);

		//* returns first byte of incoming serial data available.
		//! returns -1 if no data available
		char read();
		
		//* reads incoming buffer into char array 
		//* returns the amount of read bytes
		//? Terminates when buffer empty or if it reaches size of length
		int readBytes(char* buffer, unsigned int length);

		//* reads the Incomming buffer into a char array until the terminator
		//* returns the amount of read bytes
		//? Terminates when buffer empty or if it reaches terminator character
		//! terminator is discarded from serial buffer
		int readBytesUntil(char terminator, char* buffer, unsigned int length);
		
		//* reads the Incoming buffer into a string
		//* returns a string of bytes read
		//? Terminates when buffer empty
		std::string readString();
		
		//* reads the Incoming buffer into a string until found terminator,
		//? Terminates when buffer empty or if it reaches terminator character
		//! terminator is discarded from the serial buffer
		std::string readStringUntil(char terminator);

		//* prints data to the serial port as human-readable ASCII text
		template<typename type>
		void print(type val){
			write(std::to_string(val));
		}
		//* prints data to the serial port as human-readable ASCII text followed by a carriage return or '\r'
		template<typename type>
		void println(type val){
			write(std::to_string(val) + "\n");
		}

		//* sends a single byte
		//! will return before any characters are transmitted over serial.
		//! If the transmit buffer is full then *write()* will block until there is enough space in the buffer.
		//! To avoid blocking calls check the amount of free space in the transmit buffer using *AvailableForWrite()*
		void write(char val);
		
		//* sends a string as a series of bytes
		//! will return before any characters are transmitted over serial.
		//! If the transmit buffer is full then write() will block until there is enough space in the buffer.
		//! To avoid blocking calls check the amount of free space in the transmit buffer using *AvailableForWrite()*
		void write(std::string str);

		//* writes an array as a series of bytes
		//! will return before any characters are transmitted over serial.
		//! If the transmit buffer is full then write() will block until there is enough space in the buffer.
		//! To avoid blocking calls check the amount of free space in the transmit buffer using *AvailableForWrite()*
		void write(char* buf, unsigned int len);

	};
} // end of namespace WinSer