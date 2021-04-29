#include <iostream>
#include "WinSerial.h"

#include "common/mavlink.h"

mavlink_system_t mavlink_system = {
	1, // System ID (1-255)
	1  // Component ID (a MAV_COMPONENT value)
};

mavlink_status_t status;
mavlink_message_t msg;
int chan = MAVLINK_COMM_0;

WinSer::Serial serial("COM1", WinSer::SerParam(WinSer::SerParam::B57600));

void receive_message(){
	while(serial.available() > 0){
		uint8_t byte = serial.read();
		if(mavlink_parse_char(chan, byte, &msg, &status)){ // only returns 1 on complete and successful decoding
			std::cout << "Received message with ID: " << msg.msgid << " Sequence: " << msg.seq << " from component: " << msg.compid << " of system: " << msg.sysid << std::endl;
		}
		// error handler?
	}
}

void send_message(){

}

int main(){
	

	return 0;
}