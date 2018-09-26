/***************************************************************************
 *   Copyright (C) 2006 by P. Putnik                                       *
 *   pp@ppest.org                                                          *
 *   new QT bindings by F. Heisig 2018                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "drimgwidgetbase.h"
#include "ui_drimgwidgetbase.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <qfileinfo.h>
#include <QMessageBox>
#include <QFileDialog>

//#include <klocale.h>
//#include <kfiledialog.h>


char act = 0;
char abortf = 0;
int  detCount;

#ifdef WINDOWS
int  physDevList[MAX_DRIVE_COUNT];
char logDevMap[MAX_DRIVE_COUNT][MAX_DRIVE_COUNT];
unsigned long physSecSize[MAX_PHYS_DEVICES];
unsigned long physSeekOffset[MAX_PHYS_DEVICES];
long long physSeekPosition[MAX_PHYS_DEVICES];
#endif
char detDev[MAX_DRIVE_COUNT][DRIVE_NAME_LENGHT];
char physd[DRIVE_NAME_LENGHT];
int  selected = 111; //set to non existing drive for begin
int  form = 0; //Format 0-raw, 1-hdf, 2-hdf256
int  Filesys = 0; // 0-unknown, 1-DOS, 2-PlusIDE, 3-PlusIDE 256 // 4-7 Charea ;  11-GemDos ; 13-FAT16 partition itself
char Swf=0 ; // Sub filesystem 1-Swapped High/Low
int  Fsub = 0;  // Swap request flag - by read or write drive
char loadedF[256];
char segflag = 0;

bool ov2ro = 1;

QString fileName;
#ifdef WINDOWS
HANDLE physDevices[MAX_PHYS_DEVICES];
HANDLE finp, fout, flout;
#define FILE_OPEN_FAILED INVALID_HANDLE_VALUE
#else
FILE* physDevices[MAX_PHYS_DEVICES];
FILE *finp, *fout, *flout;
#define FILE_OPEN_FAILED NULL
#endif
unsigned long LastIOError = 0;
long long filelen;



ULONG f, g, m, u ; // general var
ULONG OffFS = 0 ;
ULONG exdl[16];
ULONG SecCnt = 1;
ULONG ChaSecCnt ;
ULONG rest;
UINT  step, cyld ;
UINT  sectr = 32;
ULONG cylc  = 1 ;
UINT  heads = 8;  //CHS parameters

//Ramsoft hdf header template:
byte rsh[128] = {0x52,0x53,0x2d,0x49,0x44,0x45,0x1a,0x10,0,0x80,0,0,0,0,0,0,0,0,0,0,0,0,
   0x8a,0x84,0xd4,3, //last two is cyl count
   0,0,8,0,0,0,
   0,2,32,0,3,0,0, 0xd4,0,0,
   32,32,32,32,0x64,0x72,0x69,0x6d,32,32,0x6d,0x61,0x64,0x65,32,32,
   32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
   32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
   32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
   32,32,32,32,32,32,32,32,32,32,
   1,0,0,0,0,3,0,0,0,2,0,0} ;

QString op, qhm;

#ifdef WINDOWS

void CloseFileX(HANDLE *f){
    SetLastError(ERROR_SUCCESS);
    if (*f == INVALID_HANDLE_VALUE)
    { return; }
    for(int i=0; i<8; i++){
       if (physDevices[i] == *f){
          physDevices[i]  = INVALID_HANDLE_VALUE;
          physSeekOffset[i]   = 0;
          physSeekPosition[i] = 0;
          physSecSize[i]  = MIN_SECTOR_SIZE;
          break;
       }
    }
    CloseHandle(*f);
    *f = INVALID_HANDLE_VALUE;
    LastIOError = GetLastError();
}
int IsPhysDevice(HANDLE fh)
{
   for(int i=0; i<8; i++){
      if(physDevices[i] == fh){ return i;}
   }
   return NO_PHYS_DRIVE;
}
DWORD SeekFileX(HANDLE fh, int offset, int origin){
   int driveNo = IsPhysDevice(fh);
   if (driveNo == NO_PHYS_DRIVE){
      return SetFilePointer(fh, offset, 0, origin);
   }
   else{
      long _offset = offset;
      if(origin == SEEK_CUR) { _offset += physSeekOffset[driveNo] + physSeekPosition[driveNo]; }
      physSeekOffset[driveNo] = _offset % physSecSize[driveNo];
      if (physSeekOffset[driveNo] > 0){
          physSeekPosition[driveNo] = _offset - physSeekOffset[driveNo];
      }
      physSeekPosition[driveNo] = _offset - physSeekOffset[driveNo];
      DWORD newpos = SetFilePointer(fh, physSeekPosition[driveNo], 0, SEEK_SET);
      if (newpos != INVALID_SET_FILE_POINTER) { newpos += physSeekOffset[driveNo]; }
      return newpos;
   }
}
long long SeekFileX64(HANDLE fh, long long offset, int origin){
   LARGE_INTEGER topos, newpos;
   int driveNo = IsPhysDevice(fh);
   if (driveNo == NO_PHYS_DRIVE){
      topos.QuadPart = offset;
      if (SetFilePointerEx(fh, topos, &newpos, origin)) {  return newpos.QuadPart; }
      return INVALID_SET_FILE_POINTER;
   }
   else{
      long long _offset = offset;
      if(origin == SEEK_CUR) { _offset += physSeekOffset[driveNo] + physSeekPosition[driveNo]; }
      physSeekOffset[driveNo] = (unsigned long)(_offset % physSecSize[driveNo]);
      if (physSeekOffset[driveNo] > 0){
          physSeekPosition[driveNo] = _offset - physSeekOffset[driveNo];
      }
      physSeekPosition[driveNo] = _offset - physSeekOffset[driveNo];
      topos.QuadPart = physSeekPosition[driveNo];
      if (SetFilePointerEx(fh, topos, &newpos, SEEK_SET)) {  return newpos.QuadPart + physSeekOffset[driveNo]; }
      return INVALID_SET_FILE_POINTER;
   }
}
unsigned char *alignBuffer(const unsigned char *buffer, unsigned long blockSize){
   return (unsigned char*)(((unsigned long)buffer + blockSize) & (~(blockSize-1))); //align it;
}
bool isAligned(const unsigned char *buffer, unsigned long blockSize){
   unsigned long rc = (unsigned long)buffer & (blockSize-1);
   return (rc == 0);
}
bool alignAble(unsigned long blockSize){
   int cnt = 0;
   for (int i=0; i<32; i++){
      if(((blockSize >> i) & 0x01) == 1){ cnt++; }
   }
   return (cnt == 1);
}

HANDLE OpenFileX(const char* name, const char * mode)
{
   DWORD CrMode = 0;
   DWORD OpMode = 0;
   HANDLE fh;

   SetLastError(ERROR_SUCCESS);
   if (strstr(mode, "r") != NULL) { CrMode = CrMode | GENERIC_READ; OpMode = OPEN_EXISTING; }
   if (strstr(mode, "w") != NULL) { CrMode = CrMode | GENERIC_WRITE; OpMode = OPEN_ALWAYS; }
   if (strstr(mode, "r+") != NULL) { CrMode = GENERIC_READ | GENERIC_WRITE; OpMode = OPEN_EXISTING; }
   if (strstr(mode, "w+") != NULL) { CrMode = GENERIC_READ | GENERIC_WRITE; OpMode = OPEN_ALWAYS; }


   fh = CreateFileA(name,
                    CrMode,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, //0,
                    NULL,
                    OpMode,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);
   LastIOError = GetLastError();
   return fh;

}
HANDLE OpenDevice(const char* name, const char * mode)
{
    DWORD CrMode = 0;
    HANDLE fh;

    SetLastError(ERROR_SUCCESS);
    if (strstr(mode, "r") != NULL) { CrMode = CrMode | GENERIC_READ; }
    if (strstr(mode, "w") != NULL) { CrMode = CrMode | GENERIC_WRITE | GENERIC_READ; } //for writing devices, read access is needed too
    if (strstr(mode, "r+") != NULL) { CrMode = GENERIC_READ | GENERIC_WRITE; }
    if (strstr(mode, "w+") != NULL) { CrMode = GENERIC_READ | GENERIC_WRITE; }

    fh = CreateFileA(name,
                     CrMode,
                     FILE_SHARE_READ | FILE_SHARE_WRITE, //0,
                     NULL,
                     OPEN_EXISTING,
                     FILE_FLAG_NO_BUFFERING, /* | FILE_FLAG_WRITE_THROUGH,*/
                     NULL);
    LastIOError = GetLastError();
    if (fh != INVALID_HANDLE_VALUE){
       DISK_GEOMETRY pdg;
       DWORD junk, pSS;
       if (DeviceIoControl(fh, IOCTL_DISK_GET_DRIVE_GEOMETRY,
                           NULL, 0, &pdg, sizeof(pdg), &junk, NULL)){
           pSS = pdg.BytesPerSector;
       } else { pSS = MIN_SECTOR_SIZE; }
       for(int i=0; i<8; i++){
          if (physDevices[i] == INVALID_HANDLE_VALUE){
             physDevices[i] = fh;
             physSecSize[i] = pSS;
             physSeekOffset[i] = 0;
             physSeekPosition[i] = 0;
             break;
          }
       }
    }
    return fh;
}

int ReadFromFile(unsigned char* buffer, size_t DataSize, size_t DataCnt, HANDLE fh){
   unsigned char *bfr, *_bfr;
   unsigned long BytesRead = 0;
   int driveNo;

   SetLastError(ERROR_SUCCESS);
   if((DataSize == 0) || (DataCnt == 0)) {return 0;}
   driveNo = IsPhysDevice(fh);
   if (driveNo == NO_PHYS_DRIVE){
      ReadFile(fh, buffer, DataSize*DataCnt, &BytesRead, NULL);
   }
   else{
      unsigned long bytesToCopy = DataCnt * DataSize;
      unsigned long bytesToRead = physSeekOffset[driveNo] + bytesToCopy;
      bool needToCopy = !((physSeekOffset[driveNo] == 0) && ((bytesToCopy % physSecSize[driveNo]) == 0) && isAligned(buffer, physSecSize[driveNo]));
      if(needToCopy){
         if ((bytesToRead % physSecSize[driveNo]) != 0) { bytesToRead = ((bytesToRead / physSecSize[driveNo]) + 1) * physSecSize[driveNo]; }
         _bfr = (unsigned char*)malloc(bytesToRead + physSecSize[driveNo]); //get Mem
         LastIOError = GetLastError();
         if (_bfr == NULL) { return 0; }
         bfr = alignBuffer(_bfr, physSecSize[driveNo]); //align it;
      } else { bfr = buffer; }
      ReadFile(fh, bfr, bytesToRead, &BytesRead, NULL);
      LastIOError = GetLastError();
      if(needToCopy){
         if (BytesRead <= physSeekOffset[driveNo]) { free(_bfr); return 0; }
         memcpy_s(buffer, bytesToCopy, &bfr[physSeekOffset[driveNo]], bytesToCopy);
         free(_bfr);
      }
      SeekFileX64(fh, physSeekPosition[driveNo] + physSeekOffset[driveNo] + BytesRead, SEEK_SET); //adjust all pointers
   }
   LastIOError = GetLastError();
   return BytesRead / DataSize;
}
int WriteToFile(unsigned char* buffer, size_t DataSize, size_t DataCnt, HANDLE fh){
    unsigned char *bfr, *_bfr;
    unsigned long BytesRead = 0;
    unsigned long BytesWritten = 0;
    int driveNo;

    SetLastError(ERROR_SUCCESS);
    if((DataSize == 0) || (DataCnt == 0)) {return 0;}
    driveNo = IsPhysDevice(fh);
    if (driveNo == NO_PHYS_DRIVE){
       WriteFile(fh, buffer, DataSize*DataCnt, &BytesWritten, NULL);
    }
    else {
       unsigned long bytesToCopy = DataCnt * DataSize;
       unsigned long bytesToWrite = physSeekOffset[driveNo] + bytesToCopy;
       bool needToCopy = !((physSeekOffset[driveNo] == 0) && ((bytesToCopy % physSecSize[driveNo]) == 0) && isAligned(buffer, physSecSize[driveNo]));
       if(needToCopy){
          if ((bytesToWrite % physSecSize[driveNo]) != 0) { bytesToWrite = ((bytesToWrite / physSecSize[driveNo]) + 1) * physSecSize[driveNo]; }
          _bfr = (unsigned char*)malloc(bytesToWrite + physSecSize[driveNo]); //get Mem
          LastIOError = GetLastError();
          if (_bfr == NULL) { return 0; }
          bfr = alignBuffer(_bfr, physSecSize[driveNo]); //align it;
          ReadFile(fh, bfr, bytesToWrite, &BytesRead, NULL);
          if(BytesRead < bytesToWrite){
             LastIOError = GetLastError();
             free(_bfr);
             return 0;
          }
          memcpy_s(&bfr[physSeekOffset[driveNo]], bytesToCopy, buffer, bytesToCopy);
       } else { bfr = buffer; }
       WriteFile(fh, bfr, bytesToWrite, &BytesWritten, NULL);
       if(needToCopy){
          if (BytesWritten <= bytesToWrite) { BytesWritten = 0; }
          free(_bfr);
       }
       SeekFileX64(fh, physSeekPosition[driveNo] + physSeekOffset[driveNo] + BytesWritten, SEEK_SET); //adjust all pointers
    }
    LastIOError = GetLastError();
    return BytesWritten / DataSize;
}
int PutCharFile(const int data, HANDLE fh){
   unsigned char b;
   SetLastError(ERROR_SUCCESS);
   b = (unsigned char)data;
   if (1 == WriteToFile(&b, 1, 1, fh)) { LastIOError = GetLastError(); return b; }
   LastIOError = GetLastError();
   return -1;
}


long long GetDeviceLengthHandle(HANDLE fh)
{
    DISK_GEOMETRY_EX pdg;
    DWORD  junk;

    SetLastError(ERROR_SUCCESS);
    BOOL bResult = DeviceIoControl(fh,
                              IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                              NULL, 0,
                              &pdg, sizeof(pdg),
                              &junk,
                              (LPOVERLAPPED) NULL);
    LastIOError = GetLastError();
    if (bResult) {
        return pdg.DiskSize.QuadPart;
    }
    return 0;
}

long long GetDeviceLength(const char* devName)
{
    HANDLE fh;
    long long devLen = 0;


    SetLastError(ERROR_SUCCESS);
    fh = CreateFileA(devName,
                     0,
                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                     NULL,
                     OPEN_EXISTING,
                     0,
                     NULL);

    LastIOError = GetLastError();
    if (fh != INVALID_HANDLE_VALUE){
        devLen = GetDeviceLengthHandle(fh);
        CloseHandle(fh);
    }
    return devLen;
}

long long GetFileLength64(HANDLE fh)
{
   DWORD hi, lo;
   LARGE_INTEGER len;

   SetLastError(ERROR_SUCCESS);
   lo = GetFileSize(fh, &hi);
   if (lo != INVALID_FILE_SIZE){
      len.HighPart = hi;
      len.LowPart  = lo;
      return len.QuadPart;
   }else {
      if (GetLastError() == ERROR_SUCCESS){
         len.HighPart = hi;
         len.LowPart  = lo;
         return len.QuadPart;
      }
   }
   return (long long)-1;
}

long GetFileLength(HANDLE fh)
{
   DWORD junk;
   return GetFileSize(fh, &junk);
}

#else
void CloseFileX(FILE** f){
   if (*f == NULL)
   { return; }
   errno = 0;
   for(int i=0; i<8; i++){
      if (physDevices[i] == *f){
         physDevices[i] = NULL;
         break;
      }
   }
   fclose(*f);
   *f = NULL;
   LastIOError = errno;
}

int ReadFromFile(unsigned char* buffer, size_t DataSize, size_t DataCnt, FILE* fh){
    errno = 0;
    if((DataSize == 0) || (DataCnt == 0)) {return 0;}
    int read = fread(buffer,DataSize,DataCnt,fh);
    LastIOError = errno;
    return read / DataSize;
}
int WriteToFile(unsigned char* buffer, size_t DataSize, size_t DataCnt, FILE* fh){
    errno = 0;
    if((DataSize == 0) || (DataCnt == 0)) {return 0;}
    int written = fwrite(buffer,DataSize,DataCnt,fh);
    LastIOError = errno;
    return written / DataSize;
}
FILE* OpenDevice(const char* name, const char * mode){
    FILE* buf = NULL;
    errno = 0;
    buf = fopen(name, mode);
    if (buf != NULL){
       setvbuf(buf, NULL, _IONBF, 0);
       for(int i=0; i<8; i++){
          if (physDevices[i] == NULL){
             physDevices[i] = buf;
             break;
          }
       }
    }
    LastIOError = errno;
    return buf;
}
FILE* OpenFileX(const char* name, const char * mode){
    FILE* buf = NULL;
    errno = 0;
    buf = fopen(name, mode);
    LastIOError = errno;
    return buf;
}

long long GetDeviceLength(const char* devName)
{
   int drih;
   long long devLen;

   drih = open(devName,  O_RDONLY | O_NONBLOCK) ;
   LastIOError = errno;
   if (drih>0) {
      devLen = lseek64( drih, 0,  SEEK_END );
      close(drih);
      return devLen;
   }
   return 0;
}

int IsPhysDevice(FILE* f)
{
   for(int i=0; i<8; i++){
      if(physDevices[i] == f){ return i;}
   }
   return NO_PHYS_DRIVE;
}

long long GetFileLength64(FILE* fh)
{
   long long pos, result;
   pos = ftell(fh);
   fseek(fh,0,SEEK_END);
   result = ftell(fh);
   fseek(fh, pos, SEEK_SET);
   return result;
}
long long GetDeviceLengthHandle(FILE* fh)
{
   return GetFileLength64(fh);
}

long GetFileLength(FILE* fh)
{
   long pos, result;
   pos = ftell(fh);
   fseek(fh,0,SEEK_END);
   result = ftell(fh);
   fseek(fh, pos, SEEK_SET);
   return result;
}

int PutCharFile(const int data, FILE* fh){
   errno = 0;
   int buf = fputc(data, fh);
   LastIOError = errno;
   return buf;
}
long SeekFileX(FILE* fh, int offset, int origin){
    errno = 0;
    if (fseek(fh, offset, origin) == 0){
        return ftell(fh);
    }
    LastIOError = errno;
    return INVALID_SET_FILE_POINTER;
}
long long SeekFileX64(FILE* fh, long long offset, int origin){
   errno = 0;
   if (fseek(fh, offset, origin) == 0){
       return ftell(fh);
   }
   LastIOError = errno;
   return INVALID_SET_FILE_POINTER;
}
int GetLastError(void){
   return errno;
}

#endif

drimgwidgetbase::drimgwidgetbase(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::drimgwidgetbase)
{
    ui->setupUi(this);
    #ifndef FRANKS_DEBUG
    ui->FileTrButton->setEnabled(false);
    #endif
    ui->readButton->setEnabled(false);
    ui->writeButton->setEnabled(false);
    #ifdef FRANKS_DEBUG
    on_openIfButton_clicked();
    #endif
}

drimgwidgetbase::~drimgwidgetbase()
{
    delete ui;
}

void drimgwidgetbase::getCHS()
{
    qhm = ui->edSecTr->text() ;
    sectr = qhm.toUInt();
    qhm = ui->edHeads->text() ;
    heads = qhm.toUInt();
    qhm = ui->edCyl->text() ;
    cylc = qhm.toLong();
}

void drimgwidgetbase::getForm()
{
    if(ui->rawRB->isChecked())  form=0;
    if(ui->hdfRB->isChecked())  form=1;
    if(ui->h256RB->isChecked()) form=2;
}

int drimgwidgetbase::detSD(char* devStr, char* volSizeMB, int* extVolumes)
{
   int e, rc = 0;
   unsigned long long drisize;
   #ifdef WINDOWS
   M_VOLUME_DISK_EXTENTS diskExtends;
   long unsigned int bytesWr;
   HANDLE devH;
   #endif

   drisize = GetDeviceLength(devStr);
   if (drisize) {
      if (extVolumes != nullptr) {
        #ifdef WINDOWS
        devH = OpenDevice(devStr, "r");
        if (devH != FILE_OPEN_FAILED) {
           if (DeviceIoControl(devH, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0, &diskExtends, sizeof(M_VOLUME_DISK_EXTENTS), &bytesWr, nullptr))
           {
              for(unsigned int i=0; i<diskExtends.NumberOfDiskExtents; i++){
                 extVolumes[i] = diskExtends.Extents[i].DiskNumber;
              }
              for(unsigned int i=diskExtends.NumberOfDiskExtents; i<MAX_DRIVES_PER_VOLUME; i++){
                 extVolumes[i] = NO_PHYS_DRIVE;
              }
           } else {
              extVolumes[0] = ERROR_CHECKING_PHYS_DRIVE;
              LastIOError = GetLastError();
           }
           CloseFileX(&devH);
           rc = 1;
        }
        #else
        rc = 0;
        #endif
      } else {
         if ((ov2ro == 0) || (drisize <= 0x100000000)){
            QString qhm;
            exdl[detCount] = drisize/512; // Sector count
            rc = 1;
            if (volSizeMB != nullptr){
               qhm.setNum((double)drisize/1048576);
               strcpy(volSizeMB, (char*)qhm.toLatin1().data());
               strcat(volSizeMB," MB");
            }
         }
      }
   }
   else{
      e = GetLastError();
      if (e == EACCES) {
         ShowErrorDialog(this, "Need to be root for accessing devices. Root?", GetLastError());
         act=0;
      }
   }
   return rc;
}
#ifdef WINDOWS
void drimgwidgetbase::detSDloop()
{
   char devStr[DRIVE_NAME_LENGHT];
   char volSize[MAX_DRIVE_COUNT][64];
   QString qhm;
   int  logCount  = 0;
   int  physCount = 0;

   int  logDriveExtend[MAX_DRIVE_COUNT][MAX_DRIVES_PER_VOLUME];
   char logDevLttr[MAX_DRIVE_COUNT];

   char devStr1[DRIVE_NAME_LENGHT] = "\\\\.\\C:";
   char devStr2[DRIVE_NAME_LENGHT] = "\\\\.\\PhysicalDrive";
   char devListName[128];

   detCount  = 0;
   act       = 1;

   ui->listBox1->clear();
   QCoreApplication::processEvents();

   //clear all
   for(int i=0; i<MAX_DRIVE_COUNT; i++){
      physDevList[i] = NO_PHYS_DRIVE;
      logDriveExtend[i][0] = NO_PHYS_DRIVE;
      for(int j=0; j<MAX_DRIVE_COUNT; j++){
         logDevMap[i][j] = '\0';
      }
   }

   //check logical devices
   for (int n=0;n<MAX_DRIVE_COUNT;n++){
      devStr1[4] = 'C'+n; //Logical device with "C:" will be sorted out later
      strcpy(devStr,devStr1);
      int c = detSD(devStr, nullptr, logDriveExtend[logCount]);
      if (logDriveExtend[logCount][0] == ERROR_CHECKING_PHYS_DRIVE){
         ShowErrorDialog(this, "DeviceIoControl error", LastIOError);
      }
      if (c) {
         logDevLttr[logCount] = 'C' + n;
         logCount++;
      }
      if (act == 0) { break; }
   }
   //check logical devices
   for (int n=0;n<MAX_DRIVE_COUNT;n++){
      sprintf(devStr,"%s%i", devStr2, n);
      volSize[physCount][0] = '\0';
      if (detSD(devStr, volSize[physCount], nullptr)){
         physDevList[physCount] = n;
         strcpy(detDev[physCount], devStr);
         physCount++;
         detCount++;
      }
   }

   //fill logDevMap
   for(int i=0; i<logCount; i++){
      if (logDriveExtend[i][0] == ERROR_CHECKING_PHYS_DRIVE){ continue; }
      for(int j=0; j<MAX_DRIVES_PER_VOLUME; j++){
         if(logDriveExtend[i][j] != NO_PHYS_DRIVE){
            int k = 0;
            while (k < physCount){
               if (physDevList[k] == logDriveExtend[i][j]){
                  int m = 0;
                  while ((logDevMap[k][m] != '\0') && (m < MAX_DRIVE_COUNT)) {
                     m++;
                  }
                  if (m < MAX_DRIVE_COUNT) {
                     logDevMap[k][m] = logDevLttr[i];
                  }
               }
               k++;
            }
         }
         else { break; }
      }
   }

   //make device list
   for(int n=0; n<physCount; n++){
      sprintf(devListName, "Drive%i [", n);
      //add all drive letters relatet to phys. count
      if (logDevMap[n][0] != '\0'){
         int l;
         int m = 0;
         while ((logDevMap[n][m] != '\0') && (m < MAX_DRIVE_COUNT)) {
            char buffer[32];
            sprintf(buffer, "%c:, ", logDevMap[n][m]);
            strcat(devListName, buffer);
            m++;
         }
         l = strlen(devListName);
         devListName[l-2] = ']';
         devListName[l-1] = '\0';
      }
      else {
         strcat(devListName, "]");
      }
      qhm = devListName;
      qhm += " ";
      qhm += volSize[n];
      ui->listBox1->addItem(qhm);
      if (qhm.contains("C:")){
         ui->listBox1->item(ui->listBox1->count()-1)->setTextColor(Qt::gray);
         ui->listBox1->item(ui->listBox1->count()-1)->setFlags(ui->listBox1->item(ui->listBox1->count()-1)->flags() & (!(Qt::ItemIsSelectable | Qt::ItemIsEnabled)));
      }
      QCoreApplication::processEvents();
   }
   act = 0;
}
#else
void drimgwidgetbase::detSDloop()
{
   char devStr[DRIVE_NAME_LENGHT];
   char volSize[64] = "";
   QString qhm;
   char devStr1[DRIVE_NAME_LENGHT] = "/dev/sda";
   char devStr2[DRIVE_NAME_LENGHT] = "/dev/mmcblk0";

   detCount  = 0;
   act       = 1;

   ui->listBox1->clear();
   QCoreApplication::processEvents();

   for (int n=0;n<MAX_DRIVE_COUNT;n++)
   {
      if (n<9) { devStr1[5] = 's' ;
         devStr1[7] = 'b'+n; //we're not checking /dev/sda
         strcpy(devStr,devStr1);
      }
      else if (n<18) { devStr1[5] = 'h' ;
         devStr1[7] = 'b'+n-9; //we're not checking /dev/hda
         strcpy(devStr,devStr1);
      }
      else {
         devStr2[11] = '0'+n-18; //but we're checking /dev/mmcblk0 to /dev/mmcblk9
         strcpy(devStr,devStr2);
      }
      int c = detSD(devStr, volSize, nullptr);
      if (c) {
         strcpy(detDev[detCount], devStr);
         qhm = detDev[detCount];
         qhm += " ";
         qhm += volSize;
         ui->listBox1->addItem(qhm);
         detCount++;
      }
      if (act == 0) { break; }
   }
   act = 0;
}
#endif

void drimgwidgetbase::on_refrButton_clicked()
{
   for(int i=0; i<MAX_PHYS_DEVICES; i++){
      #ifdef WINDOWS
      physDevices[i] = INVALID_HANDLE_VALUE;
      physSecSize[i] = MIN_SECTOR_SIZE;
      physSeekOffset[i]  = 0;
      #else
      physDevices[i] = NULL;
      #endif
   }
   ui->listBox1->setEnabled(false);
   ui->refrButton->setEnabled(false);
   QCoreApplication::processEvents();
   detSDloop() ; // Detect SCSI (USB) drives attached
   ui->listBox1->setEnabled(true);
   ui->refrButton->setEnabled(true);
   ui->FileTrButton->setEnabled(false);
   ui->readButton->setEnabled(false);
   ui->writeButton->setEnabled(false);

}

void drimgwidgetbase::on_quitButton_clicked()
{
    qApp->quit();
}

void drimgwidgetbase::on_listBox1_clicked(const QModelIndex &index)
{
    unsigned char _bufr[MIN_SECTOR_SIZE + MIN_SECTOR_SIZE]; //double for alignment
    unsigned char bufsw[MIN_SECTOR_SIZE];
    unsigned char *bufr;
             char str[128];
    QString qhm;

    bufr = alignBuffer(_bufr, MIN_SECTOR_SIZE);
    ui->FileTrButton->setEnabled(false);
    ui->readButton->setEnabled(false);
    ui->writeButton->setEnabled(false);
    ui->inf1Label->setText(" ");
    ui->inf2Label->setText(" ");
    ui->inf3Label->setText(" ");
    ui->inf4Label->setText(" ");
    ui->inf5Label->setText(" ");
    ui->inf6Label->setText(" ");
    ui->FileTrButton->setEnabled(false);
    ui->readButton->setEnabled(false);
    ui->writeButton->setEnabled(false);

    selected = index.row();
    qhm.setNum(selected);
    qhm.insert(0, "Drive: ");
    ui->inf1Label->setText(qhm); //Info about selected drive - better write here /dev...

    #ifdef WINDOWS
    if ((ui->listBox1->item(selected)->flags() & Qt::ItemIsSelectable) != Qt::ItemIsSelectable)
    {
       ui->inf3Label->setText("not selectable");
       return;
    }
    #endif

    SecCnt = exdl[selected];

    // Dev descriptor: TODO
      // Insert in EDsecC field SecCnt :

    qhm.setNum(SecCnt);
    ui->seccnEdit->setText(qhm);

    //ltoa(SecCnt,khm,10);
    //SetDlgItemText(hDlgWnd, EDsecC, khm);
    //Insert by sector count to edit boxes:

    UINT secw;
    sectr = 32;
    secw  = (SecCnt-1000)/MIN_SECTOR_SIZE ;
    heads = 2;
    if (secw>64)   heads = 4 ;
    if (secw>128)  heads = 8;
    if (secw>512)  heads = 16;
    if (secw>1024) heads = 32;
    cylc = SecCnt/(heads*sectr) ; //Sector per track is fixed to 32
    //Get sector #0 and see filesystem on drive:
       // Clear buffer ahead:
    for (int n=0;n<MIN_SECTOR_SIZE;n++) bufr[n] = 0 ;
    strcpy(physd, detDev[selected]);

    ui->inf4Label->setText(physd); //testing!!!
    finp = OpenDevice(physd,"rb");
    if (finp == FILE_OPEN_FAILED) {
      ShowErrorDialog(this, "Need to be root for accessing devices. Root?", LastIOError);
      act=0; return;
    }
    if(ReadFromFile(bufr,1,MIN_SECTOR_SIZE,finp) < MIN_SECTOR_SIZE) {
       ShowErrorDialog(this, "File/device read error", LastIOError);
       CloseFileX(&finp);
       return;
    }
    int driveNo = IsPhysDevice(finp);
    if (driveNo != NO_PHYS_DRIVE){
       qhm.setNum( physSecSize[driveNo]);
       qhm.insert(0, "Sec. size: ");
       qhm.append(" bytes");
       ui->inf5Label->setText(qhm);
    }
    CloseFileX(&finp);
    OffFS = 0 ;
    ui->secofEdit->setText("0");
    // Detecting file(partition) system used:
    Filesys = 0;
    Swf = 0 ;

    char PLUSIIS[]= "PLUSIDEDOS      ";
    char OSID[16];
    int matc = 0;
    //Get first 16 char:
    for(int n = 0; n < 16; n++ ) OSID[n]=bufr[n];
    for(int n = 0; n < 16; n++ ) {if (OSID[n]==PLUSIIS[n]) matc++ ;}
    if (matc == 16) {
        ui->inf2Label->setText(PLUSIIS);
        //SetDlgItemText(hDlgWnd, Sta2, PLUSIIS);
          //Get CHS from disk's boot sector:
        heads = bufr[34];
        sectr = bufr[35];
        cylc = bufr[32]+256*bufr[33];
    }
    //Check is 8-bit IDEDOS:
    matc = 0;
    for(int n = 0; n < 16; n++ ) OSID[n]=bufr[n*2];
    for(int n = 0; n < 16; n++ ) {if (OSID[n]==PLUSIIS[n]) matc++ ;}
    if (matc == 16) {
        ui->inf2Label->setText("PLUSIDEDOS 256 byte sect");
        heads = bufr[68];
        sectr = bufr[70];
        cylc = bufr[64]+256*bufr[66];
        // Check Hdf256:
        ui->h256RB->setChecked(1);
        ui->hdfRB->setChecked(0);
        ui->rawRB->setChecked(0);
    }
    //Put CHS values in ED boxes
    qhm.setNum(sectr);
    ui->edSecTr->setText(qhm);
    qhm.setNum(heads);
    ui->edHeads->setText( qhm );
    qhm.setNum(cylc);
    ui->edCyl->setText( qhm );

    // Check for GemDos:
    if (  ((bufr[0x1c7] == 'G') && (bufr[0x1c8] == 'E') && (bufr[0x1c9] == 'M'))
        ||((bufr[0x1c7] == 'X') && (bufr[0x1c8] == 'G') && (bufr[0x1c9] == 'M'))
        ||((bufr[0x1c7] == 'B') && (bufr[0x1c8] == 'B') && (bufr[0x1c9] == 'M')))
    {
        ui->inf2Label->setText("GemDos");
        Filesys = 11; Swf = 0;
    }
    // Check for GemDos by swapped H/L :
    for (int n=0;n<MIN_SECTOR_SIZE;n=n+2) { bufsw[n] = bufr[n+1];  bufsw[n+1] = bufr[n]; }
    if (  ((bufsw[0x1c7] == 'G') && (bufsw[0x1c8] == 'E') && (bufsw[0x1c9] == 'M'))
        ||((bufsw[0x1c7] == 'X') && (bufsw[0x1c8] == 'G') && (bufsw[0x1c9] == 'M'))
        ||((bufsw[0x1c7] == 'B') && (bufsw[0x1c8] == 'B') && (bufsw[0x1c9] == 'M')))
    {
        ui->inf2Label->setText("GemDos SW");
        Filesys = 11; Swf = 1;
    }
    // See is FAT16 partition:
    if (bufr[21] == 0xF8) {
       ui->inf2Label->setText("FAT16 part.");
       Filesys = 13;
    }
    else{
      //See is DOS mbr:
      if ((bufr[510] == 0x55) && (bufr[511] == 0xAA)){
         ui->inf2Label->setText("DOS mbr");
         Filesys = 1;
      }
      // Check for PP Charea doS
      if ((bufr[0] == 0x41) && (bufr[12] == 0x42) && (bufr[24] == 0x43)){
         strcpy(str,"Charea DoS");
         // Check is 8-bit simple IF drive/media:
         if ((bufr[0x1BC] == 'J') && (bufr[0x1BE] == 'R')){
            strcat(str," 8-bit");
            // Check Hdf256:
            ui->h256RB->setChecked(1);
            ui->hdfRB->setChecked(0);
            ui->rawRB->setChecked(0);
         }
         if ((bufr[448] == 'B') && (bufr[450] == 'S')){ strcat(str, " BigSect."); }
         ui->inf2Label->setText(str);
      }
    }
    ui->inf6Label->setText(" "); // Clear filename info
    ui->FileTrButton->setEnabled(true);
    ui->readButton->setEnabled(true);
    ui->writeButton->setEnabled(true);
}

void drimgwidgetbase::on_FileTrButton_clicked()
{
    //Filesys = 1 ; //Testing!!!!
   if (act)  { return; }
   ui->inf1Label->setText(" ");
   ui->inf2Label->setText(" ");
   ui->inf3Label->setText(" ");
   ui->inf4Label->setText(" ");
   ui->inf5Label->setText(" ");
   ui->inf6Label->setText(" ");
   ui->listBox1->clearSelection();
   ui->FileTrButton->setEnabled(false);
   ui->readButton->setEnabled(false);
   ui->writeButton->setEnabled(false);

   getForm();
   getCHS();
   segflag = 0;
   if (Filesys == 0){
       QMessageBox::information(this, "No drive selected", "Please select a drive or open an image.",QMessageBox::Cancel, QMessageBox::Cancel);
   }
   else if ((Filesys == 11) || (Filesys == 13) || (Filesys == 1)) {
      gem = new GemdDlg(this);
      gem->exec();
   }
   else {
      QMessageBox::information(this, "Not implemented yet", "Not implemented yet.",QMessageBox::Cancel, QMessageBox::Cancel);
      /*
      transferZ().exec() ;  //ZX Charea dlg call
      if (geohit) {
         qhm.setNum(sectr);
         edSecTr->setText(qhm);
         qhm.setNum(heads);
         edHeads->setText( qhm );
         cylc = SecCnt/(sectr*heads) ;
         qhm.setNum(cylc);
         edCyl->setText( qhm );
      }
      */
   }
   if (segflag){
      qhm.setNum(OffFS);
      ui->secofEdit->setText(qhm);
      qhm.setNum(ChaSecCnt);
      ui->seccnEdit->setText(qhm);
   }
    // correct CHS for case of successfull geo guess:
}

#define SML_SEC_BUF_SIZE 0x200
#define SML_SEC_BUF_HALF 0x100
#define BIG_SEC_BUF_SIZE 0x100000
#define BIG_SEC_BUF_HALF 0x080000
#define READ_ERROR   -1
#define WRITE_ERROR  -2
#define MEM_ERROR    -3
#define PARAM_ERROR  -4
#define ACCESS_ERROR -5

#ifdef WINDOWS
int CopySmallSect(HANDLE f_in, HANDLE f_out, unsigned char* bufr, unsigned char* buf2, bool swap, bool h256rb, bool fromOrig)
#else
int CopySmallSect(FILE* f_in, FILE* f_out, unsigned char* bufr, unsigned char* buf2, bool swap, bool h256rb, bool fromOrig)
#endif
{
    int status, n;
    if( swap && h256rb) { return PARAM_ERROR;}
    if (h256rb && !fromOrig){
       status = ReadFromFile(bufr,1,SML_SEC_BUF_HALF,f_in);
       if (status < SML_SEC_BUF_HALF) { return READ_ERROR;}
    } else {
       status = ReadFromFile(bufr,1,SML_SEC_BUF_SIZE,f_in);
       if (status < SML_SEC_BUF_SIZE) { return READ_ERROR;}
    }
    if (h256rb) {
        if (fromOrig) { for (n = 0; n<SML_SEC_BUF_HALF; n++) { buf2[n] = bufr[n*2]; }}
        else { for (n = 0; n<SML_SEC_BUF_HALF; n++) { buf2[n*2] = bufr[n]; }}
    }
    if (swap) { for (n = 0; n<SML_SEC_BUF_SIZE; n=n+2) { buf2[n] = bufr[n+1]; buf2[n+1] = bufr[n] ; }}
    if (swap) { if ((status = WriteToFile(buf2,1,SML_SEC_BUF_SIZE,f_out)) < SML_SEC_BUF_SIZE) {status = WRITE_ERROR;}}
    else if (h256rb) {
        if (fromOrig) { if ((status = WriteToFile(buf2,1,SML_SEC_BUF_HALF,f_out)) < SML_SEC_BUF_HALF) {status = WRITE_ERROR;}}
        else { if ((status = WriteToFile(buf2,1,SML_SEC_BUF_SIZE,f_out)) < SML_SEC_BUF_SIZE) {status = WRITE_ERROR;}}
    }
    else { if ((status = WriteToFile(bufr,1,SML_SEC_BUF_SIZE,f_out)) < SML_SEC_BUF_SIZE) {status = WRITE_ERROR;}}
    return status;
}
#ifdef WINDOWS
int CopyBigSect(HANDLE f_in, HANDLE f_out, unsigned char* bufr, unsigned char* buf2, bool swap, bool h256rb, bool fromOrig)
#else
int CopyBigSect(FILE* f_in, FILE* f_out, unsigned char* bufr, unsigned char* buf2, bool swap, bool h256rb, bool fromOrig)
#endif
{
    int status, n;
    if( swap && h256rb) { return PARAM_ERROR;}
    if (h256rb && !fromOrig){
       status = ReadFromFile(bufr,1,BIG_SEC_BUF_HALF,f_in);
       if (status < BIG_SEC_BUF_HALF) { return READ_ERROR;}
    } else {
       status = ReadFromFile(bufr,1,BIG_SEC_BUF_SIZE,f_in);
       if (status < BIG_SEC_BUF_SIZE) { return READ_ERROR;}
    }
    if (h256rb) {
        if (fromOrig) { for (n = 0; n<BIG_SEC_BUF_HALF; n++) { buf2[n] = bufr[n*2]; }}
        else { for (n = 0; n<BIG_SEC_BUF_HALF; n++) { buf2[n*2] = bufr[n]; }}
    }
    if (swap) { for (n = 0; n<BIG_SEC_BUF_SIZE; n=n+2) { buf2[n] = bufr[n+1]; buf2[n+1] = bufr[n] ; }}
    if (swap) { if((status = WriteToFile(buf2,1,BIG_SEC_BUF_SIZE,f_out)) < BIG_SEC_BUF_SIZE) {status = WRITE_ERROR;}}
    else if (h256rb) {
        if (fromOrig) { if((status = WriteToFile(buf2,1,BIG_SEC_BUF_HALF,f_out)) < BIG_SEC_BUF_HALF) {status = WRITE_ERROR;}}
        else { if((status = WriteToFile(buf2,1,BIG_SEC_BUF_SIZE,f_out)) < BIG_SEC_BUF_SIZE) {status = WRITE_ERROR;}}
    }
    else { if((status = WriteToFile(bufr,1,BIG_SEC_BUF_SIZE,f_out)) < BIG_SEC_BUF_SIZE) {status = WRITE_ERROR;}}
    return status;
}
#ifdef WINDOWS
bool drimgwidgetbase::OpenSavingFile(HANDLE* f, bool hdf)
#else
bool drimgwidgetbase::OpenSavingFile(FILE** f, bool hdf)
#endif
{
   if (hdf) {
      fileName = QFileDialog::getSaveFileName(this, "File to save", NULL, "Hdf image files (*.hdf);;All files (*.*)"); }
   else {
      fileName = QFileDialog::getSaveFileName(this, "File to save", NULL, "Raw image files (*.raw);;Img image files (*.img);;All files (*.*)");  }
   if(!fileName.isEmpty()){
      QCoreApplication::processEvents();
      *f = OpenFileX((char*)fileName.toLatin1().data(),"wb");
      if (*f == FILE_OPEN_FAILED) {
         ShowErrorDialog(this, "Saving file open error.", LastIOError);
         return false;
      } else {
         return true;
      }
   }
   return false;
}
#ifdef WINDOWS
bool drimgwidgetbase::OpenReadingFile(HANDLE* f, bool hdf)
#else
bool drimgwidgetbase::OpenReadingFile(FILE** f, bool hdf)
#endif
{
   if (hdf) {
       fileName = QFileDialog::getOpenFileName(this, "File to read", NULL, "Hdf image files (*.hdf);;All files (*.*)"); }
   else {
       fileName = QFileDialog::getOpenFileName(this, "File to read", NULL, "Raw image files (*.raw);;Img image files (*.img);;All files (*.*)"); }
   if(!fileName.isEmpty()){
      QCoreApplication::processEvents();
      *f = OpenFileX((char*)fileName.toLatin1().data(),"rb");
      if (*f == FILE_OPEN_FAILED) {
         ShowErrorDialog(this, "Reading file open error.", LastIOError);
         return false;
      } else {
         return true;
      }
   }
   return false;
}
#ifdef WINDOWS
bool drimgwidgetbase::OpenInputFile(HANDLE* f, char* Name, bool IsDevice)
#else
bool drimgwidgetbase::OpenInputFile(FILE** f, char* Name, bool IsDevice)
#endif
{
   if(IsDevice) { *f = OpenDevice(Name,"rb"); }
   else { *f = OpenFileX(Name,"rb"); }
   if (*f == FILE_OPEN_FAILED) {
      QString qhm;
      qhm.setNum(LastIOError);
      if (IsDevice) {qhm.insert(0, "Input drive open error. (");}
      else {qhm.insert(0, "Input file open error. (");}
      qhm.append(")");
      QString qhn;
      if (IsDevice) {qhn = "Device";} else {qhn = "File";};
      qhn.append(" open error.");
      QMessageBox::critical(this, qhn, qhm, QMessageBox::Cancel, QMessageBox::Cancel);
      return false;
   } else {
      return true;
   }
   return false;
}
#ifdef WINDOWS
bool drimgwidgetbase::OpenOutputFile(HANDLE* f, char* Name, bool IsDevice)
#else
bool drimgwidgetbase::OpenOutputFile(FILE** f, char* Name, bool IsDevice)
#endif
{
   if(IsDevice) { *f = OpenDevice(Name,"wb"); }
   else { *f = OpenFileX(Name,"wb"); }
   if (*f == FILE_OPEN_FAILED) {
      QString qhm;
      qhm.setNum(LastIOError);
      if (IsDevice) {qhm.insert(0, "Output drive open error. (");}
      else {qhm.insert(0, "Output file open error. (");}
      qhm.append(")");
      QString qhn;
      if (IsDevice) {qhn = "Device";} else {qhn = "File";};
      qhn.append(" open error.");
      QMessageBox::critical(this, qhn, qhm, QMessageBox::Cancel, QMessageBox::Cancel);
      return false;
   } else {
      return true;
   }
   return false;
}
#ifdef WINDOWS
long long drimgwidgetbase::CopyImage(HANDLE f_in, HANDLE f_out, ULONG SecCnt, bool swap, bool h256rb, bool fromOrig, ULONG* writtenSec)
#else
long long drimgwidgetbase::CopyImage(FILE* f_in, FILE* f_out, ULONG SecCnt, bool swap, bool h256rb, bool fromOrig, ULONG* writtenSec)
#endif
{
    int status = 0;
    int prog = 0;
    int prnxt = 1;
    long long copied = 0;
    unsigned char *bufr, *_bufr;
    unsigned char *buf2, *_buf2;

    *writtenSec = 0;
    ui->progressBar1->setValue(0);

    #ifdef WINDOWS
    int driveNo = IsPhysDevice(f_in);
    if (driveNo == NO_PHYS_DRIVE){
       driveNo = IsPhysDevice(f_out);
    }
    if (driveNo != NO_PHYS_DRIVE){
       _bufr = (unsigned char*)malloc(BIG_SEC_BUF_SIZE + physSecSize[driveNo]);
       bufr = alignBuffer(_bufr, physSecSize[driveNo]);
    }
    else
    #endif
    {
       _bufr = (unsigned char*)malloc(BIG_SEC_BUF_SIZE);
       bufr = _bufr;
    }
    if (bufr == NULL) { status = MEM_ERROR; }
    else {
       #ifdef WINDOWS
       if (driveNo != NO_PHYS_DRIVE){
          _buf2 = (unsigned char*)malloc(BIG_SEC_BUF_SIZE + physSecSize[driveNo]);
          buf2 = alignBuffer(_buf2, physSecSize[driveNo]);
       }
       else
       #endif
       {
          _buf2 = (unsigned char*)malloc(BIG_SEC_BUF_SIZE);
          buf2 = _buf2;
       }
       if (buf2 == NULL) {free(bufr); status = MEM_ERROR; }
       else {

          ULONG bg = SecCnt >> 11;
          ULONG sm = SecCnt & 0x7FF;
          setCursor(QCursor(Qt::WaitCursor));
          for( m = 0; m < bg; m++ ) {
             if ((status = CopyBigSect(f_in, f_out, bufr, buf2, swap, h256rb, fromOrig)) < 0) { break; }
             copied += status;
             *writtenSec += 0x800;
             prog = 99*(m << 11)/SecCnt;
             if (prog>prnxt) {
                #ifdef WINDOWS
                FlushFileBuffers(f_out);
                #else
                fdatasync(fileno(f_out));
                #endif
                ui->progressBar1->setValue(prog+1);
                prnxt += 1;

             }
             QCoreApplication::processEvents();
             if (abortf) { abortf=0 ; break; }
          }
          for( m = 0; m < sm; m++ ) {
             if ((status = CopySmallSect(f_in, f_out, bufr, buf2, swap, h256rb, fromOrig)) < 0) { break; }
             copied += status;
             *writtenSec += 1;
             prog = 99*m/SecCnt;
             if (prog>prnxt) {
                #ifdef WINDOWS
                FlushFileBuffers(f_out);
                #else
                fdatasync(fileno(f_out));
                #endif
                ui->progressBar1->setValue(prog+1);
                prnxt += 1;
             }
             QCoreApplication::processEvents();
             if (abortf) { abortf=0 ; break; }
          }
          setCursor(QCursor(Qt::ArrowCursor));
          free(_buf2);
       }
       free(_bufr);
    }
    if (status < 0) {
       #ifdef WINDOWS
       if (LastIOError == ERROR_ACCESS_DENIED){
          int driveNo = IsPhysDevice(f_out);
          if (driveNo != NO_PHYS_DRIVE){
             return ACCESS_ERROR;
          }
       }
       #endif
       {
       QString qhm;
       if (status == MEM_ERROR) { qhm.setNum(GetLastError()); } else { qhm.setNum(LastIOError); }
       if (status == READ_ERROR) {qhm.insert(0, "Reading file/drive error. (");}
       if (status == WRITE_ERROR){qhm.insert(0, "Writing file/drive error. (");}
       if (status == MEM_ERROR)  {qhm.insert(0, "Out of memory error. (");}
       if (status == PARAM_ERROR){qhm.insert(0, "Parameter error. (");}
       qhm.append(")\nDamn!");
       QMessageBox::critical(this, "I/O error.", qhm, QMessageBox::Cancel, QMessageBox::Cancel);
       return status;
       }
    }
    return copied;
}

void drimgwidgetbase::on_readButton_clicked()
{
   ULONG WSec = 0;
   ULONG cophh  = 0;
   ULONG SecCnt;
   QString qhm;
   long long copied = 0;

   if (act) return;
   act = 1;
   disableAll();

   //Set CHS according to Edit boxes first:
   getCHS();
   //Get OffSet and Sector count:
   qhm = ui->secofEdit->text() ;
   OffFS = qhm.toLong();
   qhm = ui->seccnEdit->text() ;
   SecCnt = qhm.toLong();
   qhm.setNum(SecCnt);
   qhm.insert(0,"Sector count: ");
   ui->inf3Label->setText(qhm);
   QCoreApplication::processEvents();

   getForm(); //Set format flag according to RB-s
   if ( selected == 99 ) { //Special case - hdf to raw or vice versa
      if ( (form==0) & (Fsub) ) // Swap L/H and save in new file
      {
         if (OpenSavingFile(&fout, false)){
            if (OpenInputFile(&finp, loadedF, false)) {
               ui->curopLabel->setText("Swapping L-H...");
               QCoreApplication::processEvents();
               long long filelen = GetDeviceLengthHandle(finp); //Filelength got
               SecCnt = filelen/512 ;
               copied = CopyImage(finp, fout, SecCnt, true, false, true, &WSec);
               //Print out info about data transfer:
               qhm = " sectors swapped L-H into file of ";
               CloseFileX(&finp);
            }
            CloseFileX(&fout);
         }
      }
      else {
         if(OpenSavingFile(&fout, (form == 0) /*contra selection here! - to make conversion*/ )){
             if (OpenInputFile(&finp, loadedF, false)) {
               //Message at bottom
               ui->curopLabel->setText("File conversion...");
               QCoreApplication::processEvents();
               if (form == 0) {  //if hdf wanted
                  //Insert CHS in hdf header:
                  rsh[28] = heads;
                  rsh[24] = cylc;
                  rsh[25] = cylc>>8;
                  rsh[34] = sectr ;
                  // if (form == 2) { //Hdf 256
                  //   rsh[8] = 1; }
                  // else   // no hdf 256 support here
                  rsh[8] = 0;
                  for(int n = 0; n < 128; n++ ) {
                     PutCharFile(rsh[n],fout);
                  }
                  cophh = 128;
               }
               else{
                  if (SeekFileX(finp,128,SEEK_SET) == INVALID_SET_FILE_POINTER){// just skip hdf header
                     ShowErrorDialog(this, "Seek error.!", INVALID_SET_FILE_POINTER);
                  }
               }
               copied = CopyImage(finp, fout, SecCnt, false, false, true, &WSec);
               //Print out info about data transfer:
               qhm = " sectors copied from img file into file of ";
               CloseFileX(&finp);
            }
            CloseFileX(&fout);
         }
      }
   }
   // Here comes read from drives, medias...:
   else{
      if (OpenSavingFile(&fout, (form > 0))){
         strcpy(physd, detDev[selected]);
         ui->inf4Label->setText(physd); //testing!!!
         if (OpenInputFile(&finp, physd, true)) {
            ui->curopLabel->setText("Reading from drive...");
            QCoreApplication::processEvents();
            if (form >0) {  //if hdf wanted
               //Insert CHS in hdf header:
               rsh[28] = heads;
               rsh[24] = cylc;
               rsh[25] = cylc>>8;
               rsh[34] = sectr ;
               rsh[8]  = (form == 2) ? 1 : 0;
               for(int n = 0; n < 128; n++ ) {
                  PutCharFile(rsh[n],fout);
               }
               cophh = 128;
            }
            if (SeekFileX(finp, OffFS*512, SEEK_SET) == INVALID_SET_FILE_POINTER){
               ShowErrorDialog(this, "File seek error.!", INVALID_SET_FILE_POINTER);
            }
            copied = CopyImage(finp, fout, SecCnt, Fsub, form == 2, true, &WSec);
            qhm = " sectors copied from device into file of ";
            CloseFileX(&finp);
         }
         CloseFileX(&fout);
      }
   }
   if (WSec == SecCnt) { ui->progressBar1->setValue(100);}
   QString num;
   num.setNum(WSec);
   qhm.insert(0,num);
   num.setNum(copied + cophh);
   qhm.append(num);
   qhm.append(" bytes.");
   ui->curopLabel->setText(qhm);
   enableAll();
   act=0;
}

void drimgwidgetbase::on_openIfButton_clicked()
{
    unsigned char bufr[640];
    unsigned char bufsw[512];
    char str[128];
    bool sethdf256 = false;
    bool sethdf512 = false;
    //bool noFSfound = false;

    if (act) { return; }
    ui->openIfButton->setEnabled(false);
    QCoreApplication::processEvents();

    #ifdef FRANKS_DEBUG
    {
      #ifdef WINDOWS
      fout = OpenFileX("D:\\Projekte\\tools\\DrImg\\testimage.img","rb");
      #else
      fout = OpenFileX("/home/frank/Projekte/ATARI/DrImg/testimage.img","rb");
      #endif
    #else
    getForm();
    if (form == 0) {
        fileName = QFileDialog::getOpenFileName(this, "File to load", NULL, "Raw image files (*.raw);;Img image files (*.img);;All files (*.*)"); }
    else {
        fileName = QFileDialog::getOpenFileName(this, "File to load", NULL, "Hdf image files (*.hdf);;All files (*.*)"); }
    if( !fileName.isEmpty() )
    {
      fout = OpenFileX((char*)fileName.toLatin1().data(),"rb");
    #endif
      if (fout == FILE_OPEN_FAILED) {
          ShowErrorDialog(this, "File open error.!", LastIOError);
          ui->openIfButton->setEnabled(true);
          return;
      }
      //Load 512+128 bytes
      //status=
      if (ReadFromFile(bufr,1,640,fout)) {}
      // get filelen
      filelen = GetFileLength64(fout);
      CloseFileX(&fout);
      #ifdef FRANKS_DEBUG
      #ifdef WINDOWS
      strcpy(loadedF, "D:\\Projekte\\tools\\DrImg\\testimage.img");
      #else
      strcpy(loadedF, "/home/frank/Projekte/ATARI/DrImg/testimage.img");
      #endif
      #else
      strcpy(loadedF, (char*)fileName.toLatin1().data());
      #endif

      ui->inf1Label->setText( "File selected" );
      selected = 99; //flag
      // Get here CHS from hdf header if...
      int ofset = 0;
      if (form > 0) ofset = 128;
      if (form>0) {
          heads = bufr[28];
          sectr = bufr[34];
          cylc = 256*bufr[25]+bufr[24] ;
          SecCnt = (filelen-ofset)/512 ;
          if ( bufr[8] == 1 ) SecCnt = (filelen-ofset)/256 ;
      }
      else { heads = 8;
          sectr = 32;
          SecCnt = filelen/512 ;
          cylc = SecCnt/(sectr*heads) ;
      }
      qhm.setNum(sectr);
      ui->edSecTr->setText(qhm);
      qhm.setNum(heads);
      ui->edHeads->setText(qhm);
      qhm.setNum(cylc);
      ui->edCyl->setText(qhm);
      qhm.setNum(SecCnt);
      ui->seccnEdit->setText(qhm);
      ui->inf5Label->setText(" ");  // Clear
      ui->secofEdit->setText( "0" );

      OffFS   = 0 ;
      Filesys = 0;
      Swf     = 0 ;
      char PLUSIIS[]= "PLUSIDEDOS      ";
      char OSID[16];
      int matc = 0;
      //Get first 16 char:
      for(int n = 0 ; n < 16; n++ ) { OSID[n]=bufr[n+ofset]; }
      for(int n = 0; n < 16; n++ ) {if (OSID[n]==PLUSIIS[n]) { matc++; }}

      if (matc == 16) {
      //Check is 8-bit IDEDOS:
         if ((form>0) && ( bufr[8] == 1 )){
            ui->inf2Label->setText( "PLUSIDEDOS 256 byte sect");
            sethdf256 = true;
         }
         else {
            ui->inf2Label->setText( PLUSIIS);
            sethdf512 = true;
         }
      }
      else {
         // Check for GemDos:

         if (  ((bufr[0x1c7] == 'G') && (bufr[0x1c8] == 'E') && (bufr[0x1c9] == 'M'))
             ||((bufr[0x1c7] == 'X') && (bufr[0x1c8] == 'G') && (bufr[0x1c9] == 'M'))
             ||((bufr[0x1c7] == 'B') && (bufr[0x1c8] == 'G') && (bufr[0x1c9] == 'M'))){
            ui->inf2Label->setText("GemDos");
            Filesys = 11 ;
         }
         // Check for GemDos by swapped H/L :
         for (int n=0;n<512;n=n+2) { bufsw[n] = bufr[n+1];  bufsw[n+1] = bufr[n]; }
         if (  ((bufsw[0x1c7] == 'G') && (bufsw[0x1c8] == 'E') && (bufsw[0x1c9] == 'M'))
             ||((bufsw[0x1c7] == 'X') && (bufsw[0x1c8] == 'G') && (bufsw[0x1c9] == 'M'))
             ||((bufsw[0x1c7] == 'B') && (bufsw[0x1c8] == 'G') && (bufsw[0x1c9] == 'M'))){
            ui->inf2Label->setText("GemDos SW");
            Filesys = 11;
            Swf = 1;
         }
         // See is FAT16 partition:
         if (bufr[21] == 0xF8) {
            ui->inf2Label->setText("FAT16 part.");
            Filesys = 13;
         }
         //See is DOS mbr:
         else if ((bufr[510+ofset] == 0x55) && (bufr[511+ofset] == 0xAA)){
            ui->inf2Label->setText( "DOS mbr");
            Filesys = 1 ;
         }
         // Check for PP Charea doS
         else if (( bufr[0+ofset] == 0x41 )  && ( bufr[12+ofset] == 0x42 )  &&  ( bufr[24+ofset] == 0x43 ))  {  //ABC
            strcpy(str, "Charea DoS 16-bit");
            if (( bufr[ofset+448] == 'B' ) && ( bufr[ofset+450] == 'S' )) { strcat(str," BigSect."); }
            ui->inf2Label->setText(str);
            sethdf512 = true;
         }
         else if (( bufr[0+ofset] == 0x41 ) && ( bufr[6+ofset] == 0x42 )  && ( bufr[12+ofset] == 0x43 )) { //ABC
            strcpy(str,"Charea DoS 8-bit");
            if (( bufr[ofset+224] == 'B' ) && ( bufr[ofset+225] == 'S' )) { strcat(str," BigSect."); }
            ui->inf2Label->setText(str);
            sethdf256 = true;
         }
         else if ( form >0 ) {
            if ( bufr[8] == 1 ){
               sethdf256 = true;
            }
            else{
               sethdf512 = true;
            }
         }
         else {
            //noFSfound = true;
         }
      }
      if (sethdf256){
         ui->h256RB->setChecked(1);
         ui->hdfRB->setChecked(0);
      }

      if (sethdf512){
         if ( form ) {
            ui->h256RB->setChecked(0);
            ui->hdfRB->setChecked(1);
         }
      }

      ui->listBox1->clearSelection();
      //  SendMessage(GetDlgItem(hDlgWnd, ListBox), LB_SETCURSEL, -1, 1) ; // remove highlight of drive selection
      qhm.setNum(SecCnt);
      qhm.insert(0, "Sector count: ") ;
      ui->inf3Label->setText(qhm);
      //Print out loaded filename:
      ui->inf6Label->setText(fileName);
      //if (!noFSfound){
      ui->FileTrButton->setEnabled(true);
      //}
   }
   ui->openIfButton->setEnabled(true);
}

void drimgwidgetbase::on_writeButton_clicked()
{
   ULONG fsecc;
   ULONG cophh = 0;

   if (act) { return; }
   act = 1;
   disableAll();
   getForm() ;
   if( OpenReadingFile(&finp,(form > 0)) )
   {
      QCoreApplication::processEvents();
      if ( selected==99 ) { qhm = "Write to selected file!"; }
      else {
         qhm.setNum(selected);
         qhm.insert(0, "Write to selected drive ");
         qhm.append(": ");
         qhm.append(detDev[selected]);
      }
      qhm.append("\nAre you sure?");
      // File to drive
      if (QMessageBox::question(this, "Writing", qhm, QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::No)
      { enableAll(); act=0; return; }
      if ( selected==99) {
         OpenOutputFile(&fout, loadedF, false);
      }
      else{
         strcpy(physd, detDev[selected]);
         if (( ov2ro ) && ( SecCnt>0x800000)) {
            QMessageBox::critical(this, "Read only.", "Read only for large drives!\nStop",QMessageBox::Cancel, QMessageBox::Cancel);
            enableAll();
            act=0;
            return;
         }
         OpenOutputFile(&fout, physd, true);
      }
      if (fout != FILE_OPEN_FAILED){ // cannot open the drive
         if ( selected==99) ui->curopLabel->setText("Writing to file...");
         else ui->curopLabel->setText("Writing to drive...");

         //SecCnt = exdl[selected];  //Already set **** - Maybe changed.... see it
         if (form > 0) {  //if hdf to load
            cophh = 128;
         }

         long long filelen = GetFileLength64(finp);
         //Filelength got

         if(SeekFileX(finp, cophh, SEEK_SET) == INVALID_SET_FILE_POINTER){ // Set to 0 by raw, or to 128 by hdf
            ShowErrorDialog(this, "Seek error.!", INVALID_SET_FILE_POINTER);
         }
         if (form == 2)  fsecc = (filelen-cophh)/256 ;
         else fsecc = (filelen-cophh)/512 ;
         qhm = ui->seccnEdit->text() ;
         SecCnt = qhm.toLong();
         if (!((selected==99) && (SecCnt == 0)) && (SecCnt < fsecc))  fsecc = SecCnt;
         qhm.setNum(fsecc);
         ui->inf4Label->setText(qhm) ; // testing!!!

         // Need to get value from edLine!!!
         qhm = ui->secofEdit->text() ;
         OffFS = qhm.toLong();
         if(SeekFileX(fout, OffFS*512, SEEK_SET ) == INVALID_SET_FILE_POINTER){
            ShowErrorDialog(this, "Seek error.!", INVALID_SET_FILE_POINTER);
         }
         ULONG WSec = 0;
         long long copied = CopyImage(finp, fout, fsecc, Fsub, form == 2, false, &WSec);
         #ifdef WINDOWS
         if (copied == ACCESS_ERROR){
            QMessageBox::question(this, "Access error", "It seems there is a mounted partition in the drive. \nMounting is automatically done by Windows, so you need to erase this partition before writing to drive. \nUse disk management tool or do it by command line (read READ.ME').", QMessageBox::Cancel, QMessageBox::Cancel);
            qhm = "";
            ui->curopLabel->setText(qhm);
            if (WSec == fsecc) { ui->progressBar1->setValue(0);}
         } else
         #endif
         {
         setCursor(QCursor(Qt::ArrowCursor));
         //Print out info about data transfer:
         qhm.setNum(copied);
         qhm.append(" bytes (");
         QString num;
         num.setNum(WSec);
         qhm.append(num);
         if (selected==99) qhm.append(" sectors ) written to file.");
         else qhm.append(" sectors ) written to drive.");
         ui->curopLabel->setText(qhm);
         if (WSec == fsecc) { ui->progressBar1->setValue(100);}
         }
         CloseFileX(&fout);
      }
      CloseFileX(&finp);
   }
   enableAll();
   act = 0;
}
void drimgwidgetbase::on_abortButton_clicked()
{
   if (act) abortf = 1 ;
}

void drimgwidgetbase::on_creimfButton_clicked()
{
   int  status;
   int  secsize;
   unsigned char bufr[512];

   if (act) { return; }
   act=1;

   getForm(); //Set format flag according to RB-s
   // Get CHS parameters from EDboxes:
   getCHS();    // CHS too
   SecCnt = sectr*heads*cylc;

   qhm.setNum(SecCnt);
   ui->seccnEdit->setText( qhm );

   //inf5Label->setText( " ");  // Clear
   OffFS = 0 ;
   ui->secofEdit->setText( "0" );
   if( OpenSavingFile(&fout,(form > 0)) )  {
      //update() ;
      int prog = 0;
      int prnxt = 1;
      ui->curopLabel->setText( "Creating image file...");
      ui->progressBar1->setValue( 0 );
      setCursor(QCursor(Qt::WaitCursor));
      QCoreApplication::processEvents();
      m = 0 ;
      if ( form > 0 ) { // HDF images
         // Set values in HDF header:
         rsh[28] = heads;
         rsh[24] = cylc;
         rsh[25] = cylc>>8;
         rsh[34] = sectr ;
         rsh[8] = (form == 2) ? 1 : 2; //Hdf 256
         status = WriteToFile(rsh,1,128,fout);
         if (status != 128) { /*todo*/ }
         m = m+128 ;
      }
      secsize = ( form == 2 ) ? 256 : 512;
      // Just make file with totalsectors*512 (or 256) 0 bytes
      //clear buffer:
      for (int n=0;n<secsize;n++){ bufr[n] = 0 ; }
      g = sectr*heads*cylc ;
      for (f=0;f<g;f++) {
         status = WriteToFile(bufr,1,secsize,fout);
         if (status != secsize) { /*todo*/ }
         QCoreApplication::processEvents();
         m = m+secsize ;
         if (abortf) { abortf=0; break; }
         //u = m/secsize ;
         prog = (100*(m/secsize))/SecCnt;
         if (prog>prnxt) {
            ui->progressBar1->setValue( prog+1 );
            prnxt++ ;
         }
      }  // some check needed!
      //Close file:
      CloseFileX(&fout);
      setCursor(QCursor(Qt::ArrowCursor));
      qhm.setNum(m);
      qhm.insert(0, "Image file of ");
      qhm.append(" bytes created.");
      ui->curopLabel->setText(qhm);
   } // if filesel end
   act=0;
}
void drimgwidgetbase::disableAll()
{
    ui->openIfButton->setEnabled(false);
    ui->creimfButton->setEnabled(false);
    ui->FileTrButton->setEnabled(false);
    ui->readButton->setEnabled(false);
    ui->refrButton->setEnabled(false);
    ui->writeButton->setEnabled(false);
    ui->listBox1->setEnabled(false);
    QCoreApplication::processEvents();
}
void drimgwidgetbase::enableAll()
{
    ui->openIfButton->setEnabled(true);
    ui->creimfButton->setEnabled(true);
    ui->FileTrButton->setEnabled(true);
    ui->readButton->setEnabled(true);
    ui->refrButton->setEnabled(true);
    ui->writeButton->setEnabled(true);
    ui->listBox1->setEnabled(true);
}


void drimgwidgetbase::on_ov2roCB_clicked()
{
   // Set flag according to CB state:
   if( ui->ov2roCB->isChecked() ) ov2ro = 1;
   else ov2ro = 0 ;
}

void drimgwidgetbase::on_swapCB_clicked()
{
   if( ui->swapCB->isChecked() ) Fsub = 1;
   else Fsub = 0 ;
}

