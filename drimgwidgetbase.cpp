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

char devStr[13];
char dstr[60]; //for string operations
int  detCount;
char detDev[13][16] ;
char physd[13];
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
HANDLE physDevices[8] = {0,0,0,0,0,0,0,0};
HANDLE finp, fout, flout;
#define FILE_OPEN_FAILED INVALID_HANDLE_VALUE
#else
FILE* physDevices[8] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
FILE *finp, *fout, *flout;
#define FILE_OPEN_FAILED NULL
#endif
long LastIOError = 0;
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

void CloseFileX(HANDLE f){
    for(int i=0; i<8; i++){
       if (physDevices[i] == f){
          physDevices[i] = 0;
          break;
       }
    }
    CloseHandle(f);
}

HANDLE OpenFileX(const char* name, const char * mode)
{
   DWORD CrMode = 0;
   DWORD OpMode = 0;
   HANDLE fh;

   if (strstr(mode, "r") != NULL) { CrMode = CrMode | GENERIC_READ; OpMode = OPEN_EXISTING; }
   if (strstr(mode, "w") != NULL) { CrMode = CrMode | GENERIC_WRITE; OpMode = OPEN_ALWAYS; }
   if (strstr(mode, "r+") != NULL) { CrMode = GENERIC_READ | GENERIC_WRITE; OpMode = OPEN_EXISTING; }
   if (strstr(mode, "w+") != NULL) { CrMode = GENERIC_READ | GENERIC_WRITE; OpMode = OPEN_ALWAYS; }


   fh = CreateFileA(name,
                    CrMode,
                    0,
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

    if (strstr(mode, "r") != NULL) { CrMode = CrMode | GENERIC_READ; }
    if (strstr(mode, "w") != NULL) { CrMode = CrMode | GENERIC_WRITE; }


    fh = CreateFileA(name,
                     CrMode,
                     0, //FILE_SHARE_READ | FILE_SHARE_WRITE,
                     NULL,
                     OPEN_EXISTING,
                     FILE_FLAG_NO_BUFFERING,// | FILE_FLAG_WRITE_THROUGH,
                     NULL);
    LastIOError = GetLastError();
    if (fh != INVALID_HANDLE_VALUE){
       for(int i=0; i<8; i++){
          if (physDevices[i] == 0){
             physDevices[i] = fh;
             break;
          }
       }
    }
    return fh;
}

int ReadFromFile(unsigned char* buffer, size_t DataSize, size_t DataCnt, HANDLE fh){
   DWORD BytesRead = 0;
   if((DataSize == 0) || (DataCnt == 0)) {return 0;}
   ReadFile(fh, buffer, DataSize*DataCnt, &BytesRead, NULL);
   LastIOError = GetLastError();
   return BytesRead / DataSize;
}
int WriteToFile(unsigned char* buffer, size_t DataSize, size_t DataCnt, HANDLE fh){
    DWORD BytesWritten = 0;
    if((DataSize == 0) || (DataCnt == 0)) {return 0;}
    WriteFile(fh, buffer, DataSize*DataCnt, &BytesWritten, NULL);
    LastIOError = GetLastError();
    return BytesWritten / DataSize;
}
int PutCharFile(const int data, HANDLE fh){
   unsigned char b;
   b = (unsigned char)data;
   if (1 == WriteToFile(&b, 1, 1, fh)) { LastIOError = GetLastError(); return b; }
   LastIOError = GetLastError();
   return -1;
}


long long GetDeviceLengthHandle(HANDLE fh)
{
    DISK_GEOMETRY_EX pdg;
    DWORD  junk;

    BOOL bResult = DeviceIoControl(fh,
                              IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                              NULL, 0,
                              &pdg, sizeof(pdg),
                              &junk,
                              (LPOVERLAPPED) NULL);
    if (bResult) {
        return pdg.DiskSize.QuadPart;
    }
    return 0;
}

long long GetDeviceLength(const char* devName)
{
    HANDLE fh;
    long long devLen = 0;


    fh = CreateFileA(devName,
                     0,
                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                     NULL,
                     OPEN_EXISTING,
                     0,
                     NULL);

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
bool IsPhysDevice(HANDLE fh)
{
   for(int i=0; i<8; i++){
      if(physDevices[i] == fh){ return true;}
   }
   return false;
}
long SeekFileX(HANDLE fh, int offset, int origin){
   return SetFilePointer(fh, offset, 0, origin);
}
long long SeekFileX64(HANDLE fh, long long offset, int origin){
   LARGE_INTEGER topos, newpos;

   topos.QuadPart = offset;
   if (SetFilePointerEx(fh, topos, &newpos, origin)) {  return newpos.QuadPart; }
   return -1;
}

#else
void CloseFileX(FILE* f){
   for(int i=0; i<8; i++){
      if (physDevices[i] == f){
         physDevices[i] = NULL;
         break;
      }
   }
   fclose(f);
}

int ReadFromFile(unsigned char* buffer, size_t DataSize, size_t DataCnt, FILE* fh){
    if((DataSize == 0) || (DataCnt == 0)) {return 0;}
    errno = 0;
    int read = fread(buffer,DataSize,DataCnt,fh);
    LastIOError = errno;
    return read / DataSize;
}
int WriteToFile(unsigned char* buffer, size_t DataSize, size_t DataCnt, FILE* fh){
    if((DataSize == 0) || (DataCnt == 0)) {return 0;}
    errno = 0;
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
   if (drih>0) {
      devLen = lseek64( drih, 0,  SEEK_END );
      close(drih);
      return devLen;
   }
   return 0;
}

bool IsPhysDevice(FILE* f)
{
   for(int i=0; i<8; i++){
      if(physDevices[i] == f){ return true;}
   }
   return false;
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
   int buf = fputc(data, fh);
   LastIOError = errno;
   return buf;
}
long SeekFileX(FILE* fh, int offset, int origin){
    if (fseek(fh, offset, origin) == 0){
        return ftell(fh);
    }
    return -1;
}
long long SeekFileX64(FILE* fh, long long offset, int origin){
   if (fseek(fh, offset, origin) == 0){
       return ftell(fh);
   }
   return -1;
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

int drimgwidgetbase::detSD()
{
   int e, rc = 0;
   unsigned long long drisize;

   drisize = GetDeviceLength(devStr);
   if (drisize) {
      finp = OpenDevice(devStr,"wb"); // CD ROMs will not open
      if (finp != FILE_OPEN_FAILED){
         exdl[detCount] = drisize/512; // Sector count
         rc = 1;
         qhm.setNum((double)drisize/1048576);
         strcat(dstr, (char*)qhm.toLatin1().data());
         strcat(dstr," MB");
         CloseFileX(finp);
      }
   }
   else{
      e = GetLastError();
      if (e == EACCES) {
         QMessageBox::critical(this, "Drive open error.", "Need to be root for accessing devices. Root?", QMessageBox::Cancel, QMessageBox::Cancel);
         act=0;
      }
   }
   return rc;
}

void drimgwidgetbase::detSDloop()
{
   #ifdef WINDOWS
   char devStr1[7] = "\\\\.\\E:";
   #else
   char devStr1[9]  = "/dev/sda";
   char devStr2[13] = "/dev/mmcblk0";
   #endif

   ui->listBox1->clear();
   detCount = 0;
   act      = 1;
   for (int n=0;n<20;n++)
   {
      #ifdef WINDOWS
      devStr1[4] = 'E'+n; //we're not checking /dev/sda and /dev/sdb
      strcpy(devStr,devStr1);
      #else
      if (n<6) { devStr1[5] = 's' ;
         devStr1[7] = 'c'+n; //we're not checking /dev/sda and /dev/sdb
         strcpy(devStr,devStr1);
      }
      else if (n<12) { devStr1[5] = 'h' ;
         devStr1[7] = 'c'+n-6; //we're not checking /dev/hda and /dev/hdb
         strcpy(devStr,devStr1);
      }
      else {
         devStr2[11] = '0'+n-12; //but we're checking /dev/mmcblk0 to /dev/mmcblk7
         strcpy(devStr,devStr2);
      }
      #endif
      strcpy(dstr,devStr);
      strcat(dstr,"  ");

      int c = detSD();
      if (act == 0) { break; }

      if (c) {
      ui->listBox1->addItem(QString(dstr));
      if (n<12){
         for (int k=0;k<9;k++) detDev[k][detCount]=devStr[k]; // Store dev path
            for (int k=9;k<13;k++) detDev[k][detCount]='\0';
         }
         else{
            for (int k=0;k<13;k++) detDev[k][detCount]=devStr[k]; // Store dev path
         }
         detCount++;
      }
   }
   act = 0;
}

void drimgwidgetbase::on_refrButton_clicked()
{
    detSDloop() ; // Detect SCSI (USB) drives attached
/*
    ui->listBox1->addItem(QString("--- item 1 ---"));
    ui->listBox1->addItem(QString("--- item 2 ---"));
    ui->listBox1->addItem(QString("--- item 3 ---"));
*/
}



void drimgwidgetbase::on_quitButton_clicked()
{
    qApp->quit();
}

void drimgwidgetbase::on_listBox1_clicked(const QModelIndex &index)
{
    unsigned char bufr[512];
    unsigned char bufsw[512];

    selected = index.row();

    qhm.setNum(selected);
    qhm.insert(0, "Drive: ");
    ui->inf1Label->setText(qhm); //Info about selected drive - better write here /dev...
    ui->inf2Label->setText(" ");

    SecCnt = exdl[selected];
    ui->inf3Label->setText(" ");   //Clear info line 3

    // Dev descriptor: TODO
      // Insert in EDsecC field SecCnt :

    qhm.setNum(SecCnt);
    ui->seccnEdit->setText(qhm);

    //ltoa(SecCnt,khm,10);
    //SetDlgItemText(hDlgWnd, EDsecC, khm);
    //Insert by sector count to edit boxes:

    UINT secw;
    sectr = 32;
    secw  = (SecCnt-1000)/512 ;
    heads = 2;
    if (secw>64)   heads = 4 ;
    if (secw>128)  heads = 8;
    if (secw>512)  heads = 16;
    if (secw>1024) heads = 32;
    cylc = SecCnt/(heads*sectr) ; //Sector per track is fixed to 32
    //Get sector #0 and see filesystem on drive:
       // Clear buffer ahead:
    for (int n=0;n<512;n++) bufr[n] = 0 ;
    for (int k=0;k<13;k++) physd[k] = detDev[k][selected];

    ui->inf4Label->setText(physd); //testing!!!
    finp = OpenDevice(physd,"rb");
    if (finp == FILE_OPEN_FAILED) {
      QMessageBox::critical(this, "Drive open error.", "Need to be root for accessing devices. Root?", QMessageBox::Cancel, QMessageBox::Cancel);
      act=0; return;
    }

    if(ReadFromFile(bufr,1,512,finp) < 512) {
       QString qhm;
       qhm.setNum(GetLastError());
       qhm.insert(0, "Code: ");
       QMessageBox::information(this, "read error", qhm ,QMessageBox::Cancel, QMessageBox::Cancel);
    }
    CloseFileX(finp);
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
    for (int n=0;n<512;n=n+2) { bufsw[n] = bufr[n+1];  bufsw[n+1] = bufr[n]; }
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
         strcpy(dstr,"Charea DoS");
         // Check is 8-bit simple IF drive/media:
         if ((bufr[0x1BC] == 'J') && (bufr[0x1BE] == 'R')){
            strcat(dstr," 8-bit");
            // Check Hdf256:
            ui->h256RB->setChecked(1);
            ui->hdfRB->setChecked(0);
            ui->rawRB->setChecked(0);
         }
         if ((bufr[448] == 'B') && (bufr[450] == 'S')){ strcat(dstr, " BigSect."); }
         ui->inf2Label->setText(dstr);
      }
    }
    ui->inf6Label->setText(" "); // Clear filename info
}

void drimgwidgetbase::on_FileTrButton_clicked()
{
    //Filesys = 1 ; //Testing!!!!
   if (act)  { return; }
   getForm();
   getCHS();
   segflag = 0;
   if ((Filesys == 11) || (Filesys == 13) || (Filesys == 1)) {
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
      ui->inf3Label->setText(" "); // Clear selected sec count message
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
#define READ_ERROR  -1
#define WRITE_ERROR -2
#define MEM_ERROR   -3
#define PARAM_ERROR -4

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
         QString qhm;
         qhm.setNum(GetLastError());
         qhm.insert(0, "Saving file open error. (");
         qhm.append(")\nDamn!");
         QMessageBox::critical(this, "File open error.", qhm, QMessageBox::Cancel, QMessageBox::Cancel);
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
         QString qhm;
         qhm.setNum(GetLastError());
         qhm.insert(0, "Reading file open error. (");
         qhm.append(")\nDamn!");
         QMessageBox::critical(this, "File open error.", qhm, QMessageBox::Cancel, QMessageBox::Cancel);
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
      qhm.setNum(GetLastError());
      if (IsDevice) {qhm.insert(0, "Input drive open error. (");}
      else {qhm.insert(0, "Input file open error. (");}
      qhm.append(")\nDamn!");
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
      qhm.setNum(GetLastError());
      if (IsDevice) {qhm.insert(0, "Output drive open error. (");}
      else {qhm.insert(0, "Output file open error. (");}
      qhm.append(")\nDamn!");
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
    unsigned char *bufr;
    unsigned char *buf2;

    *writtenSec = 0;
    ui->progressBar1->setValue(0);
    bufr = (unsigned char*)malloc(BIG_SEC_BUF_SIZE);
    if (bufr == NULL) { status = MEM_ERROR; }
    else {
       buf2 = (unsigned char*)malloc(BIG_SEC_BUF_SIZE);
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
                #ifndef WINDOWS
                //if (IsPhysDevice(f_out)){
                   fdatasync(fileno(f_out));
                   fflush(f_out);
                //}
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
                #ifndef WINDOWS
                //if (IsPhysDevice(f_out)){
                   fdatasync(fileno(f_out));
                   fflush(f_out);
                //}
                #endif
                ui->progressBar1->setValue(prog+1);
                prnxt += 1;
             }
             QCoreApplication::processEvents();
             if (abortf) { abortf=0 ; break; }
          }
          setCursor(QCursor(Qt::ArrowCursor));
          free(buf2);
       }
       free(bufr);
    }
    if (status < 0) {
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
               CloseFileX(finp);
            }
            CloseFileX(fout);
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
                  SeekFileX(finp,128,0) ; // just skip hdf header
               }
               copied = CopyImage(finp, fout, SecCnt, false, false, true, &WSec);
               //Print out info about data transfer:
               qhm = " sectors copied from img file into file of ";
               CloseFileX(finp);
            }
            CloseFileX(fout);
         }
      }
   }
   // Here comes read from drives, medias...:
   else{
      if (OpenSavingFile(&fout, (form > 0))){
         for (int k=0;k<13;k++){ physd[k] = detDev[k][selected]; }
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
            SeekFileX(finp, OffFS*512, SEEK_SET);
            copied = CopyImage(finp, fout, SecCnt, Fsub, form == 2, true, &WSec);
            qhm = " sectors copied from device into file of ";
            CloseFileX(finp);
         }
         CloseFileX(fout);
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
   act=0;
}

void drimgwidgetbase::on_openIfButton_clicked()
{
    unsigned char bufr[640];
    unsigned char bufsw[512];

    if (act) { return; }
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
          QMessageBox::critical(this, "File open error.", "File open error.\nDamn!",QMessageBox::Cancel, QMessageBox::Cancel);
          return;
      }
      //Load 512+128 bytes
      //status=
      if (ReadFromFile(bufr,1,640,fout)) {}
      // get filelen
      filelen = GetFileLength64(fout);
      CloseFileX(fout);
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
            goto sethdf256 ;
         }
         else {
            ui->inf2Label->setText( PLUSIIS);
            goto sethdf512 ;
         }
      }
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
         goto removh;
      }
      //See is DOS mbr:
      if ((bufr[510+ofset] == 0x55) && (bufr[511+ofset] == 0xAA)){
         ui->inf2Label->setText( "DOS mbr");
         Filesys = 1 ;
         goto removh ;
      }
      // Check for PP Charea doS
      if (( bufr[0+ofset] == 0x41 )  && ( bufr[12+ofset] == 0x42 )  &&  ( bufr[24+ofset] == 0x43 ))  {  //ABC
         strcpy(dstr, "Charea DoS 16-bit");
         if (( bufr[ofset+448] == 'B' ) && ( bufr[ofset+450] == 'S' )) { strcat(dstr," BigSect."); }
         ui->inf2Label->setText( dstr);
         goto sethdf512 ;
      }
      if (( bufr[0+ofset] == 0x41 ) && ( bufr[6+ofset] == 0x42 )  && ( bufr[12+ofset] == 0x43 )) { //ABC
         strcpy(dstr,"Charea DoS 8-bit");
         if (( bufr[ofset+224] == 'B' ) && ( bufr[ofset+225] == 'S' )) { strcat(dstr," BigSect."); }
         ui->inf2Label->setText( dstr);
         goto sethdf256 ;
      }
      if ( form >0 ) {
        if ( bufr[8] == 1 ){
           goto sethdf256;
         }
         else{
            goto sethdf512;
         }
      }
      goto removh;

sethdf256:
      ui->h256RB->setChecked(1);
      ui->hdfRB->setChecked(0);
      goto removh ;

sethdf512:
      if ( form ) {
         ui->h256RB->setChecked(0);
         ui->hdfRB->setChecked(1);
      }

removh:
      ui->listBox1->clearSelection();
      //  SendMessage(GetDlgItem(hDlgWnd, ListBox), LB_SETCURSEL, -1, 1) ; // remove highlight of drive selection
      qhm.setNum(SecCnt);
      qhm.insert(0, "Sector count: ") ;
      ui->inf3Label->setText(qhm);
      //Print out loaded filename:
      ui->inf6Label->setText(fileName );
   }
}

void drimgwidgetbase::on_writeButton_clicked()
{
   ULONG fsecc;
   ULONG cophh = 0;

   if (act) { return; }
   act = 1;
   getForm() ;
   if( OpenReadingFile(&finp,(form > 0)) )
   {
      QCoreApplication::processEvents();
      if ( selected==99 ) { qhm = "Write to selected file!"; }
      else {
         qhm.setNum(selected);
         qhm.insert(0, "Write to selected drive ");
      }
      qhm.append("\nAre you sure?");
      // File to drive
      if (QMessageBox::question(this, "Writing", qhm, QMessageBox::Yes|QMessageBox::No, QMessageBox::No) == QMessageBox::No)
      { act=0; return; }
      if ( selected==99) {
         OpenOutputFile(&fout, loadedF, false);
      }
      else{
         for (int k=0; k<9; k++) physd[k] = detDev[k][selected];
         if (( ov2ro ) && ( SecCnt>0x20000000)) {
            QMessageBox::critical(this, "Read only.", "Read only for large drives!\nStop",QMessageBox::Cancel, QMessageBox::Cancel);
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

         SeekFileX(finp, cophh, SEEK_SET) ; // Set to 0 by raw, or to 128 by hdf
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
         SeekFileX(fout, OffFS*512, SEEK_SET );
         ULONG WSec = 0;
         long long copied = CopyImage(finp, fout, fsecc, Fsub, form == 2, false, &WSec);
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
         CloseFileX(fout);
      }
      CloseFileX(finp);
   }
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
      CloseFileX(fout);
      setCursor(QCursor(Qt::ArrowCursor));
      for(int n = 0; n < 60; n++ ) dstr[n] = 0; //Clear string
       qhm.setNum(m);
       qhm.insert(0, "Image file of ");
       qhm.append(" bytes created.");
       ui->curopLabel->setText(qhm);
   } // if filesel end
   act=0;
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

