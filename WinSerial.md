# Single header Serial library

## WinSer:

* Winser::Buffer
    * SerialBufferException
    * struct __buffer

* Exception SerialException
* struct SerParam
    * enum Baudrate
    * enum StopBits
    * enum Parity
* struct SerTimeOut
* class Serial

# Serial class
Serial manager class. Manages a read and send thread and two buffers containing incoming and outgoing buffers. 
Serial communications in windows are based on a HANDLE object 

## functions
```c++
Serial(char* Port, SerParam SP, SerTimeOut ST)
```
**Description:** serial object constructor. <br>
**return value:** - <br>
* **Port:** contains the name of the serial port this object is connected to. <br>
* **SP:** contains the Serial Parameters for the Serial connection. E.g. baudrate, stopbits & Parity.<br>
* **ST:** contains the Serial Timeout settings for the serial connection.
```c++
unsinged int available()
```
**Description:** checks the amount of available bytes in incoming buffer.<br>
**return value:** The number of bytes available to read from the incoming buffer. 
```c++
unsigned int availableForWrite()
```
**Description:** checks the amount of available bytes in outgoing buffer without blocking the write operation. <br>
**return value:** The number of bytes avaialable to write.
```c++
void flush(unsigned int refreshrate)
```
**Description:** waits for the transmission of outgoing serial data to complete <br>
**return value:** - <br>
* **refreshrate:** is the time between checks if the outgoing buffer is empty in milliseconds.
```c++
void clearBuffer()
```
**Description:** clears the currently stored buffer data of the incoming and outgoing buffer <br>
**return value:** -
```c++
void setTimeout(SerTimtOut ST)
```
**Description:** sets the timeout parameters of the serial port. <br>
**return value:** -
* **ST**: contains the new Serial Timeout settings
```c++
char read()
```
**Description:** reads the first available byte of incoming serial data. <br>
**return value:** the first byte of incoming buffer. <br>
_throws SerialException if no data is available_
```c++
int readBytes(char* buffer, unsigned int length)
```
**Description:** reads the incoming buffer into buffer char array.<br>
**return value:** number of characters read.<br>
_Terminates when buffer empty or if it reaches size of **length**_
* **buffer:** the buffer that will contain the read bytes
* **length:** amount of characters to read, 0 = entire buffer
```c++
int readBytesUntil(char terminator, char* buffer, int length)
```
**Description:** reads the incoming into buffer char array. <br>
**return value:** number of characters read.<br>
_terminates when buffer empty, if it reaches **terminator** charactor, or until it reaches **length**_.<br>
_terminator is added to character buffer_.
* **terminator:** the termination character
* **buffer:** the character buffer to be read into
* **length:** the maximum amount of characters to be read before exiting
```c++
std::string readString()
```
**Description:** reads the incoming buffer into a string.<br>
**return value:** The bytes read as a string.<br>
_Terminates when buffer empty_
```c++
std::string readStringUntil(char terminator)
```
**Description:** reads the incoming buffer into a string until terminator character is found<br>
**return value:** the bytes read as a string<br>
_Terminates when buffer empty or if it reaches terminator character_.<br>
_terminator is added to string_.
* **terminator:** the termination character
```c++
void write(char val)
void write(std::string str)
void write(char* buf, unsigned int len)
```
**Description:** writes a character to the outgoing buffer if outgoing buffer is full it will retry every 5ms for 10 times. Afterwhich if it failed to write it will throw a SerialBufferException<br>
**return value:** -
* **val:** the character to be written to the buffer
* **str:** the string to be written to the buffer
* **buf:** the character buffer to be written to the buffer
* **len:** the size of the character buffer to be written to the buffer