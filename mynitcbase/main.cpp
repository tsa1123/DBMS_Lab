#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include<cstring>
#include<iostream>
using namespace std;
int main(int argc, char *argv[]) {
  /* Initialize the Run Copy of Disk */
  Disk disk_run;
  // StaticBuffer buffer;
  // OpenRelTable cache;
  unsigned char buffer[BLOCK_SIZE];
  Disk::readBlock(buffer,7777);
  memcpy(buffer,"hello",6);
  Disk::writeBlock(buffer,7777);
  
  unsigned char buffer2[BLOCK_SIZE];
  Disk::readBlock(buffer2,7777);
  cout<<buffer2<<endl;

  for(int i=0;i<4;i++){
  	Disk::readBlock(buffer2,i);
	for(int j=0;j<BLOCK_SIZE;j++){
		cout<<to_string(buffer2[j]);
	}
	cout<<endl;
  }
  //return FrontendInterface::handleFrontend(argc, argv);
}
