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

#include "gemddlg.h"
#include "ui_gemddlg.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDirIterator>
#include <QCloseEvent>
#include <QTreeView>
#include <QtGui>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef WINDOWS
 #include <time.h>
 #include <windows.h>
 #include <sys/stat.h>
#else
 #include <utime.h>
#endif
#include <sys/stat.h>

typedef unsigned long  ULONG;
typedef unsigned int   UINT;

extern int Filesys; // 0-unknown, 1-DOS, 2-PlusIDE, 3-PlusIDE 256 // 4-7 Charea ;  11-GemDos
extern char Swf ; // Sub filesystem 1-Swapped High/Low
extern int Fsub ;  // Swap request flag - by read or write drive
extern QString fileName;
extern QString op, qhm;
extern ULONG filelen, u ;
extern ULONG ChaSecCnt ;

extern int selected;
extern char detDev[13][16] ;
extern char physd[13];
extern char loadedF[256] ;
extern int form;
extern int clust ;
extern int status ;

extern char segflag ;
extern ULONG SecCnt ;
extern ULONG OffFS;
extern QString op, qhm;
extern char dstr[60];
extern UINT sectr ;
extern ULONG cylc ;
extern UINT heads ;
extern bool ov2ro;
/*
extern unsigned char bufr[640];
extern unsigned char  buf2[524];
extern unsigned char  bufsw[524];
extern QString litet;
*/
#ifdef WINDOWS
extern HANDLE OpenFileX(const char* name, const char * mode);
extern HANDLE OpenDevice(const char* name, const char * mode);
extern void CloseFileX(HANDLE f);
extern int ReadFromFile(unsigned char* buffer, size_t DataSize, size_t DataCnt, HANDLE fh);
extern int WriteToFile(unsigned char* buffer, size_t DataSize, size_t DataCnt, HANDLE fh);
extern int PutCharFile(const int data, HANDLE fh);
extern long long GetDeviceLengthHandle(HANDLE fh);
extern long long GetDeviceLength(const char* devName);
extern long long GetFileLength64(HANDLE fh);
extern long GetFileLength(HANDLE fh);
extern long SeekFileX(HANDLE fh, int offset, int origin);
extern long long SeekFileX64(HANDLE fh, long long offset, int origin);
#else
extern FILE* OpenDevice(const char* name, const char * mode);
extern FILE* OpenFileX(const char* name, const char * mode);
extern void CloseFileX(FILE* f);
extern int ReadFromFile(unsigned char* buffer, size_t DataSize, size_t DataCnt, FILE* fh);
extern int WriteToFile(unsigned char* buffer, size_t DataSize, size_t DataCnt, FILE* fh);
extern int PutCharFile(const int data, FILE* fh);
extern long long GetDeviceLengthHandle(FILE* fh);
extern long long GetDeviceLength(const char* devName);
extern long long GetFileLength64(FILE* fh);
extern long GetFileLength(FILE* fh);
extern long SeekFileX(FILE* fh, int offset, int origin);
extern long long SeekFileX64(FILE* fh, long long offset, int origin);
extern int GetLastError(void);
#endif

#ifdef WINDOWS
static HANDLE fhdl, fhout;
#define FILE_OPEN_FAILED INVALID_HANDLE_VALUE
#else
static FILE *fhdl = nullptr, *fhout = nullptr;
#define FILE_OPEN_FAILED NULL
#endif

ULONG epos, eposIn, stfilelen, copied ;
ULONG Gpartp[24] ; // GemDos part postions
ULONG Gparsiz[24] ; // GemDos part sizes
ULONG  dirflen ;
int reculev, selcnt, selP, j, d, e ;
int fdat, ftim ;
unsigned int oe, zz;
unsigned int dpos, fatTime, fatDate;
long tosave;

int           PartSubDirLevel;
unsigned int  PartSectorSize;
unsigned int  PartReservedSectors ; // Used clusters, free clusters of partition
unsigned int  PartSectorsPerFAT;
unsigned int  PartRootDirSectors;
unsigned int  PartSectorsPerCluster;
unsigned int  PartHeadSectors;
unsigned int  PartFirstRootDirSector;
unsigned long PartTotalSectors;
unsigned long PartTotalDataClusters;
unsigned long PartStartPosition;

const unsigned long DIR_ENTRIES_MAX_CNT  = 512;
unsigned int  DirCurrentStartCluster;
unsigned int  DirCurrentClustes[98] ; // For storing DIR cluster -> Points To Sector
unsigned int  DirCurrentLenght;
unsigned int  DirCurrentClusterCnt;
unsigned int  DirParentStartCluster;
unsigned int  DirEntryPos[DIR_ENTRIES_MAX_CNT] ;
unsigned int  DirEntrySortPos[DIR_ENTRIES_MAX_CNT];
unsigned int  DirEntrySortIndex[DIR_ENTRIES_MAX_CNT];
unsigned int  DirEntryCnt;
QStringList   DirRawFilelist;

#define MAX_SUBDIR_DEPTH 16
unsigned int  pDirEntryPos[MAX_SUBDIR_DEPTH] ; // Position in parent dir, by recursive extraction
unsigned int  credirClu[16] ; // Created DIR cluster spread store
unsigned int  credirCnt;
unsigned char dirbuf[512*100];
unsigned char PartFATbuffer[132000] ;
unsigned char trasb[65536] ;  // 128 sector
char subnam[12][MAX_SUBDIR_DEPTH] ;
char subdnam[13];
char subpath[256];
char DestDir[256];
char res ;
bool timestCur=0;
const unsigned long INVALID_VALUE = 0xFFFFFFFF;
const unsigned long LAST_CLUSTER  = 0xFFFF;

int DirLevel;

#ifdef FRANKS_DEBUG
unsigned char* debug_dirbuf;
#endif

GemdDlg::GemdDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GemdDlg)
{
    ui->setupUi(this);
    ui->AddFiles->setIcon(QIcon(":/Icons/AddFiles.png"));
    ui->desallP->setIcon(QIcon(":/Icons/DeselectAll.png"));
    ui->DeleteFiles->setIcon(QIcon(":/Icons/DeleteFiles.png"));
    ui->ExtractFiles->setIcon(QIcon(":/Icons/Extract.png"));
    ui->opdirP->setIcon(QIcon(":/Icons/AddDir.png"));
    ui->quitP->setIcon(QIcon(":/Icons/Quit.png"));
    ui->setddP->setIcon(QIcon(":/Icons/DestDir.png"));
    ui->newfP->setIcon(QIcon(":/Icons/OpenFolder.png"));
    OpenDialog();
}

GemdDlg::~GemdDlg()
{
    delete ui;
}

void GenerateFileName(unsigned char* buf, int offs, QString *name)
{
   char nstr[60];

   name->clear();
   for(int k=0; k<8; k++) {
      nstr[k] = buf[offs+k] ;
   }
   nstr[8] = '.' ;
   nstr[9] = buf[offs+8];
   nstr[10] = buf[offs+9];
   nstr[11] = buf[offs+10];
   nstr[12] = ' ';
   nstr[13] = ' ';
   nstr[14] = 0 ;
   int o = buf[offs+11] ;
   if ( (o & 16) == 16 ) {
      name->append("DIR");
   }
   else if ((o & 8) == 8) {
      name->append("VOL");
   }
   else {
      // File lenght in bytes from DIR fields:
      unsigned int dirflen = 16777216*buf[offs+31]+65536*buf[offs+30]+256*buf[offs+29]+buf[offs+28] ;
      name->setNum(dirflen);
   }
   name->insert(0, nstr);
}

void GemdDlg::OpenDialog()
{
   int p;
   unsigned long m, n;
   unsigned char bufr[512];
   unsigned char buf2[512];

   DestDir[0] = '\0';
   // Open file or physical drive:
   if (selected<16){
      for (int k=0;k<9;k++) { physd[k] = detDev[k][selected]; }
      if ((ov2ro)  && ( SecCnt>0x20000000)) {
         fhdl = OpenDevice(physd, "rb" );
      }
      else{
         fhdl = OpenDevice(physd, "r+b" );
      }
   }
   else{
      fhdl = OpenFileX(loadedF, "r+b" );
   }
   if (fhdl == FILE_OPEN_FAILED) {
      QMessageBox::critical(this, "Error", "Drive/file open error ", QMessageBox::Cancel, QMessageBox::Cancel);
      return; //gemdpex
   }
   // Load bootsector
   ReadFromFile(bufr, 1, 512, fhdl);
   if ( Filesys == 13 ){
      Gpartp[0] = 0 ;
      m = bufr[19]+256*bufr[20] ;
      if ( m == 0 ) {
         m = bufr[32]+bufr[33]*256 + bufr[34]*65536 +bufr[35]*16777216 ;
         qhm = "BIGDOS part. ";
      }
      else{
         qhm = "DOS16 part. ";
      }
      n = (bufr[11]+256*bufr[12])>>9 ;   // Sector size
      Gparsiz[0] = m ;
      QString num;
      num.setNum(Gparsiz[0]/2*n);
      qhm.append(num);
      qhm.append(" KB");
      ui->partLB->addItem(qhm);
   }
   // Detect partition system - if DOS (PC) then look only DOS16 and BIGDOS partitions
   // And in extend. partitions
   if ( Filesys == 1 ) {
      p = 0 ; // Partition counter (index)
      for (int i=0x1BE;i<0x1EF;i=i+16) { // Four primary slots
         if (( bufr[i+4] == 6 ) || ( bufr[i+4] == 4 )) {
            // Position, absolute, in sectors
            Gpartp[p] = bufr[i+8]+bufr[i+9]*256 + bufr[i+10]*65536 +bufr[i+11]*16777216 ;
            // Get size:
            Gparsiz[p] = bufr[i+12]+bufr[i+13]*256 + bufr[i+14]*65536 +bufr[i+15]*16777216 ;
            // Not looking CHS entries!
            // Size in KB:
            qhm.setNum(Gparsiz[p]/2);
            if ( bufr[i+4] == 6 ) {  // BigDOS code
               qhm.insert(0, "BigDOS prim.: ");
            }
            if ( bufr[i+4] == 4 ) {  // DOS16 code
               qhm.insert(0, "DOS16 prim.: ");
            }
            qhm.append(" KB");
            ui->partLB->addItem(qhm);
            p++ ;
            if ( p> 24 ){ break; }
         } //dlookext
         if ( bufr[i+4] == 5 ) {  // Extended
            // Position, absolute, in sectors
            epos = bufr[i+8]+bufr[i+9]*256 + bufr[i+10]*65536 +bufr[i+11]*16777216 ;
            do{
               strcpy(dstr,"Ext. ");
               SeekFileX64(fhdl, epos*512, 0);
               ReadFromFile(buf2, 1, 512, fhdl);
               // First entry must be regular partition
               if (( buf2[0x1C2] == 6 ) || ( buf2[0x1C2] == 4 )){
                  //Get pos, absolute:
                  Gpartp[p] =  epos + buf2[0x1C6]+buf2[0x1C7]*256 + buf2[0x1C8]*65536 +buf2[0x1C9]*16777216 ;
                  // Get size:
                  Gparsiz[p] = buf2[0x1CA]+buf2[0x1CB]*256 + buf2[0x1CC]*65536 +buf2[0x1CD]*16777216 ;
                  // Size in KB:
                  qhm.setNum(Gparsiz[p]/2);
                  if ( buf2[0x1C2] == 6 ) {  // BigDOS code
                     qhm.insert(0, "BigDOS: ");
                  }
                  if ( buf2[0x1C2] == 4 ) {  // DOS16 code
                     qhm.insert(0, "DOS16: ");
                  }
                  qhm.append(" KB");
                  ui->partLB->addItem(qhm) ;
                  //SendMessage(GetDlgItem(hDlgWnd, GemP), LB_ADDSTRING, 0, (LPARAM) dstr );
                  p++ ;
                  if ( p> 24 ) break ;
                  // Look for further Extended slots:
               } //dlookext2:
               if ( buf2[0x1D2] == 5 ) {
                  epos = epos+buf2[0x1D6]+buf2[0x1D7]*256 + buf2[0x1D8]*65536 +buf2[0x1D9]*16777216 ;
               }
            }while( buf2[0x1D2] == 5 );
            if ( buf2[0x1D2] == 0 ) break ; // End of chain
         }
      } // i loop end
   } //if end
   // See is swapped High/Low... TODO
   // Detecting GemDos partitions
   else {
      if (Swf)
      {   // Swap low/high bytes:
         unsigned char k;
         for (n=0;n<512;n=n+2)
         { k=bufr[n] ; bufr[n]=bufr[n+1];bufr[n+1]=k; }
      }
      p = 0 ; // Partition counter (index)
      for (int i=0x1C6;i<0x1F7;i=i+12) { // Four primary slots
         if (  ((bufr[i+1] == 'G') & (bufr[i+2] == 'E') & (bufr[i+3] == 'M')) | ((bufr[i+1] == 'B') & (bufr[i+2] == 'G') & (bufr[i+3] == 'M'))) {
            // Primary GemDos part
            // Position, absolute, in sectors
            Gpartp[p] = bufr[i+7]+bufr[i+6]*256 + bufr[i+5]*65536 +bufr[i+4]*16777216 ;
            // Get size:
            Gparsiz[p] = bufr[i+11]+bufr[i+10]*256 + bufr[i+9]*65536 +bufr[i+8]*16777216 ;
            // Size in KB:
            qhm.setNum(Gparsiz[p]/2);
            qhm.insert(0, "Primary: ");
            qhm.append(" KB");
            ui->partLB->addItem(qhm);
            p++;
            if (p > 24) { break; }
         }
         if ((bufr[i+1] == 'X') && (bufr[i+2] == 'G') && (bufr[i+3] == 'M')) {
            // Extended GemDos slot
            // Position, absolute, in sectors
            eposIn = bufr[i+7]+bufr[i+6]*256 + bufr[i+5]*65536 +bufr[i+4]*16777216 ;
            epos = eposIn ;
            bool reloop;
            do{
               reloop = false;
               SeekFileX64(fhdl, epos*512, 0);
               ReadFromFile(buf2, 1, 512, fhdl);
               if (Swf) {
                  // Swap low/high bytes:
                  unsigned char k;
                  for (n=0;n<512;n=n+2){
                     k=buf2[n];
                     buf2[n]=buf2[n+1];
                     buf2[n+1]=k;
                  }
               }
               // First entry must be regular partition
               if ( ((buf2[0x1C7] == 'G') & (buf2[0x1C8] == 'E')  &  (buf2[0x1C9] == 'M'))| ((buf2[0x1C7] == 'B') & (buf2[0x1C8] == 'G')  &  (buf2[0x1C9] == 'M')) ) {
                  //Get pos, absolute:
                  Gpartp[p] = epos + buf2[0x1CD]+buf2[0x1CC]*256 + buf2[0x1CB]*65536 +buf2[0x1CA]*16777216 ;
                  // Get size:
                  Gparsiz[p] = buf2[0x1D1]+buf2[0x1D0]*256 + buf2[0x1CF]*65536 +buf2[0x1CE]*16777216 ;
                  // Size in KB:
                  qhm.setNum(Gparsiz[p]/2);
                  qhm.insert(0, "Extended: ");
                  qhm.append(" KB");
                  ui->partLB->addItem(qhm);
                  //SendMessage(GetDlgItem(hDlgWnd, GemP), LB_ADDSTRING, 0, (LPARAM) dstr );
                  p++;
                  if ( p > 24 ) { break; }
                  // Look for further Extended slots:
                  if ((buf2[0x1D3] == 'X') && (buf2[0x1D4] == 'G') && (buf2[0x1D5] == 'M')) {
                     epos = eposIn+buf2[0x1D9]+buf2[0x1D8]*256 + buf2[0x1D7]*65536 +buf2[0x1D6]*16777216 ;
                     reloop = true;
                  }
               }
            }while(reloop);
            if ( p > 24 ) { break; }
         }
      } // i loop end
   } // else end
}

void GemdDlg::on_partLB_clicked(const QModelIndex &index)
{
   unsigned int  n, m;
   unsigned char k;
   unsigned char buf2[512];

   selP = index.row();
   //Load bootsector of partition:
   PartStartPosition =  Gpartp[selP]*512 ;
   SeekFileX64(fhdl, PartStartPosition, 0);
   ReadFromFile(buf2, 1, 512, fhdl);
   //lseek(drih, PartStartPosition, 0 );
   //read(drih, buf2, 512);
   if (Swf)
   {   // Swap low/high bytes:
       for (unsigned int n=0;n<512;n=n+2)
       { k=buf2[n] ; buf2[n]=buf2[n+1];buf2[n+1]=k; }
   }
   // Get BPB parameters:
   PartSectorSize         = buf2[11]+256*buf2[12] ;
   PartSectorsPerFAT      = buf2[22] ;
   n = PartSectorSize>>9 ;
   PartRootDirSectors     = (buf2[17]/16 + 16*buf2[18])/n ;
   PartSectorsPerCluster  = buf2[13] ;
   PartReservedSectors    = buf2[14]+256*buf2[15] ;
   PartHeadSectors        = PartReservedSectors+PartSectorsPerFAT*2+PartRootDirSectors ;
   PartFirstRootDirSector = PartReservedSectors+PartSectorsPerFAT*2 ;
   PartTotalSectors= buf2[19]+256*buf2[20] ;
   if ( PartTotalSectors == 0 ){
       PartTotalSectors = buf2[32]+buf2[33]*256 + buf2[34]*65536 +buf2[35]*16777216 ;
   }
   PartTotalDataClusters = (PartTotalSectors-PartHeadSectors)/PartSectorsPerCluster ;
   // Display begin sector and count of sectors of selected part. :
   #ifdef FRANKS_DEBUG
   qhm.setNum( Gpartp[selP] );
   qhm.insert(0, "start sec.: ");
   qhm.append(", sec. cnt: ");
   QString num;
   if ( Filesys == 13) {
      num.setNum( PartTotalSectors );
   }
   else {
       num.setNum( Gparsiz[selP] );
   }
   qhm.append(num);
   ui->partinfLab->setText(qhm);
   #endif
   // Load FAT #1
   SeekFileX64(fhdl, PartStartPosition+PartSectorSize*PartReservedSectors, 0);
   ReadFromFile(PartFATbuffer, 1, PartSectorsPerFAT*PartSectorSize, fhdl);
   if (Swf)
   {   // Swap low/high bytes:
      for (m=0;m<PartSectorsPerFAT*PartSectorSize;m=m+2){
         k=PartFATbuffer[m] ; PartFATbuffer[m]=PartFATbuffer[m+1];PartFATbuffer[m+1]=k;
      }
   }
   loadroot(true) ;
}
void GemdDlg::on_filesLB_clicked(const QModelIndex &index)
{
    tm  DateTime;
    unsigned int dpos;
    int i, l, listOffset;
    QChar* buf;

    listOffset = (PartSubDirLevel > 0) ? 1 : 0;
    dpos = DirEntrySortPos[index.row()-listOffset];
    QString qhm;
    GenerateFileName(dirbuf, dpos, &qhm);
    l = qhm.length();
    buf = qhm.data();
    for(i=l-1;i>=0;i--){
       if (i>12){ qhm.remove(i,1); }
       else{ if(buf[i] == ' '){ qhm.remove(i,1); } }
    }
    l = qhm.length();
    if(buf[l-1] == '.') {qhm.remove(l-1,1); }
    GetFATFileDateTime(dirbuf, dpos, &DateTime);
    qhm.append(" ");
    QString num;
    num.setNum(DateTime.tm_year + 1900);
    qhm.append(num);
    if (DateTime.tm_mon < 10){ qhm.append("/0"); } else { qhm.append("/"); }
    num.setNum(DateTime.tm_mon);
    qhm.append(num);
    if (DateTime.tm_mday < 10){ qhm.append("/0"); } else { qhm.append("/"); }
    num.setNum(DateTime.tm_mday);
    qhm.append(num);
    if (DateTime.tm_hour < 10){ qhm.append(" 0"); } else { qhm.append(" "); }
    num.setNum(DateTime.tm_hour);
    qhm.append(num);
    if (DateTime.tm_min < 10){ qhm.append(":0"); } else { qhm.append(":"); }
    num.setNum(DateTime.tm_min);
    qhm.append(num);
    if (DateTime.tm_sec < 10){ qhm.append(":0"); } else { qhm.append(":"); }
    num.setNum(DateTime.tm_sec);
    qhm.append(num);
    ui->errti->setText(qhm);
}

unsigned int GetFreeClusters(void)
{
   unsigned int i = 0 ;
   unsigned short fx;

   for(unsigned int n = 4; n < (2*PartTotalDataClusters+4);  n = n+2 ) {
      fx = PartFATbuffer[n]+256*PartFATbuffer[n+1] ;
      if ( fx == 0 ) i++  ;
   }
   return(i);
}

void GemdDlg::ShowPartitionUsage()
{
   unsigned int  SecsFact;
   unsigned long UsedClusters, FreeClusters;

   SecsFact = PartSectorSize/512 ;
   FreeClusters = GetFreeClusters();
   UsedClusters = PartTotalDataClusters-FreeClusters;
   qhm.setNum( (UsedClusters*PartSectorsPerCluster)/2*SecsFact);
   qhm.insert(0,"Used: ");
   qhm.append(" KB  Free: ");
   QString num;
   num.setNum( (FreeClusters*PartSectorsPerCluster)/2*SecsFact );
   qhm.append(num);
   qhm.append(" KB");
   ui->usfrLabel->setText(qhm);
}

void FATlookUp(unsigned int StartCluster, unsigned int* NextCluster, unsigned int* FileSector) // For FAT 16 MS DOS
{
  int fp = 2*StartCluster  ; // 2 times because it' a byte-array holding words
  *NextCluster = PartFATbuffer[fp]+256*PartFATbuffer[fp+1] ;
  *FileSector = PartHeadSectors+(StartCluster-2)*PartSectorsPerCluster ; //first sector of cluster
}
void FATfreeCluster(unsigned int Cluster)
{
  int fp = 2*Cluster  ; // 2 times because it' a byte-array holding words
  PartFATbuffer[fp] = 0;
  PartFATbuffer[fp+1] = 0;
}

int ReadSectors( int StartSector, int Count, unsigned char *Buffer)
{
   int d,e,j;
   unsigned char buf[512];

   SeekFileX64(fhdl, PartStartPosition+StartSector*PartSectorSize, 0);
   //lseek(drih, PartStartPosition+StartSector*PartSectorSize, 0 );
   if (Swf) { j = Count*PartSectorSize/512 ;
      for (e=0;e<j;e++)
      {
         ReadFromFile(buf, 1, 512, fhdl);
         //read(drih, buf, 512);
         for (d=0;d<512;d=d+2) { Buffer[d+e*512]=buf[d+1] ; Buffer[d+1+e*512]=buf[d]; }
      }
   }
   else {
      ReadFromFile(Buffer, 1, Count*PartSectorSize, fhdl);
      //read(drih, Buffer, Count*PartSectorSize);
   }
   return 1;
}
unsigned long GemdDlg::WriteSectors( int StartSector, int Count, unsigned char *Buffer)
{
    unsigned char buf[512];
    unsigned int u, written = 0;
    bool failed;

    SeekFileX64(fhdl, PartStartPosition+StartSector*PartSectorSize, 0);
    //lseek(drih, PartStartPosition+StartSector*PartSectorSize, 0 );
    if (Swf) {
       j = Count*PartSectorSize/512;
       u = 0;
       for (e=0;e<j;e++) {
          for (d=0;d<512;d=d+2) {
              buf[d+1]=Buffer[d+e*512] ;
              buf[d]=Buffer[d+1+e*512];
          }
          u = WriteToFile(buf, 1, 512, fhdl);
          if ((failed = (u < 512)) == true) break;
          written = written + u;
       }
    }
    else {
       written = WriteToFile(Buffer, 1, Count*PartSectorSize, fhdl);
       failed = (written < Count*PartSectorSize);
    }
    if (failed) {
       QString qhm;
       qhm.setNum(errno/*GetLastError()*/);
       qhm.insert(0, "Drive/file write error. (");
       qhm.append(")");
       QMessageBox::critical(this, "Error", qhm, QMessageBox::Cancel, QMessageBox::Cancel);
    }
    return written;
}
void GemdDlg::LoadDirBuf(unsigned int *StartCluster, unsigned char* DirBuffer)
{
   int n = 0;
   unsigned int NextCluster, FileSector;
   unsigned long CurrentBufOffs = 0;

   do{ //fatop2:
      FATlookUp(*StartCluster, &NextCluster, &FileSector) ;
      //load cluster
      // Store loaded cluster start sector numbers for further writes!!
      DirCurrentClustes[n] = FileSector ;
      res = ReadSectors(FileSector, PartSectorsPerCluster, dirbuf+CurrentBufOffs);
      CurrentBufOffs = CurrentBufOffs+PartSectorsPerCluster*PartSectorSize ;
      n++;
      if ( NextCluster>0xFFF0 ){ break; }// For FAT 16
      *StartCluster = NextCluster ;
   }while(CurrentBufOffs<512*98);
   //subdirf:
   DirCurrentLenght = n*PartSectorsPerCluster ; // Current DIR length in sectors
   DirCurrentClusterCnt = n;
   //Clear following sector for sure
   for (int o=0; o<512; o++){ DirBuffer[CurrentBufOffs+o] = 0; }
}
void GemdDlg::WriteCurrentDirBuf(void)
{
   unsigned long CurrentBufOffs = 0;

   if ( PartSubDirLevel == 0 ) WriteSectors( PartFirstRootDirSector, PartRootDirSectors, dirbuf);
   else  {
      for (unsigned int i=0; i<DirCurrentClusterCnt; i++){
         WriteSectors( DirCurrentClustes[i], PartSectorsPerCluster, dirbuf+CurrentBufOffs);
         CurrentBufOffs = CurrentBufOffs+PartSectorsPerCluster*PartSectorSize;
      }
   }
}

int LoadEntryName(unsigned char* DirBuffer, int EntryPos, bool IsRoot, QString* Name)
{
   if (DirBuffer[EntryPos] == 0) return 1 ;  // Skip empty
   if (DirBuffer[EntryPos] == 0xE5) return 2 ; // Skip deleted
   if ((!IsRoot) && (DirBuffer[EntryPos] == 0x2E)) return 2 ; // Skip subdir marks
   GenerateFileName(DirBuffer, EntryPos, Name);
   return 0 ;
}

static const unsigned char TOSfield[19] = {6,7,14,15,16,17,18,19,20,21,22,23,24,27,28,29,30,31,0};
static const unsigned char DOSfield[19] = {208,209,210,211,224,225,226,227,228,229,230,231,232,233,234,235,236,237,0};

void PadTOStoLocal(char* Name){
    //remove garbage
    for(int i=0; i<12; i++){
       if ((unsigned char)Name[i] > 126) {
           Name[i] = '_';
       }
    }
    // pad TOS specials
    for(int i=0; i<12; i++){
      if (Name[i] == 0) { break; }
      for(int j=0; j<18;j++){
         if((unsigned char)Name[i] == TOSfield[j]){
            Name[i] = DOSfield[j];
            break;
         }
      }
   }
}
void PadLocalToTOS(char* Name){
   for(int i=0; i<12; i++){
      if (Name[i] == 0) { break; }
      for(int j=0; j<18;j++){
         if((unsigned char)Name[i] == DOSfield[j]){
            Name[i] = TOSfield[j];
            break;
         }
      }
   }
}
void GemdDlg::LoadSubDir(bool IsRoot, bool doList)
{
   unsigned int n, i;
   QString Name;

   DirEntryCnt = 0 ;
   DirRawFilelist.clear();
   for (n=0;n<DirCurrentLenght*PartSectorSize; n=n+32) {
      int rc = LoadEntryName(dirbuf, n, IsRoot, &Name);
      if (rc == 1) break;
      if (rc == 2) continue;
      DirRawFilelist.append(Name);
      DirEntryPos[DirEntryCnt] = n ; // Store pos (bytewise) in DIR
      DirEntryCnt++;
      if (DirEntryCnt >= DIR_ENTRIES_MAX_CNT) { break; }
   }
   for(i=DirEntryCnt;i<DIR_ENTRIES_MAX_CNT;i++){ DirEntryPos[i] = 0; }
   SortFATNames(dirbuf, DirEntryPos, DirEntrySortPos, DirEntrySortIndex);

   if (doList){
      ui->filesLB->clear();
      if (!IsRoot) ui->filesLB->addItem("..");
      for(i=0; i<DirEntryCnt; i++){
          ui->filesLB->addItem(DirRawFilelist.value(DirEntrySortIndex[i]));
      }
   }
   QCoreApplication::processEvents();
}
void GemdDlg::loadroot(bool doList)
{
   SeekFileX64(fhdl, PartStartPosition+PartFirstRootDirSector*PartSectorSize, 0);
   ReadFromFile(dirbuf, 1, PartRootDirSectors*PartSectorSize, fhdl);
   if (Swf)
   {
      // Swap low/high bytes:
      unsigned char k;
      for (unsigned long m=0;m<PartRootDirSectors*PartSectorSize;m=m+2){
         k=dirbuf[m] ; dirbuf[m]=dirbuf[m+1];dirbuf[m+1]=k;
      }
   }
   // List files from ROOT dir:
   if(doList) { ui->dirLabel->setText("ROOT"); }
   DirCurrentLenght = PartRootDirSectors ;
   PartSubDirLevel = 0;
   DirParentStartCluster = 0 ;
   DirCurrentLenght = PartRootDirSectors; // Current DIR length in sectors
   LoadSubDir(true, doList);
   ShowPartitionUsage() ;
}
void GemdDlg::subdirh(bool doList)
{
   int k;

   dstr[11] = 0 ;
   DirParentStartCluster = DirCurrentStartCluster ; // Keep this value for SubDIR creation
   if ( PartSubDirLevel ) {
      int spafl, c = 0;
      subpath[0] = '/';
      for(int o=0;o<PartSubDirLevel;o++){
         spafl = 0;
         for(int i=0;i<11;i++) {
            k = subnam[i][o+1] ;
            if(k){
               if (k == ' ' ){
                  spafl++;
               }
               else {
                  if ( spafl>0 ) { subpath[c++] = '.' ; spafl = -111 ; }
                  subpath[c++] = k ;
               }
            }
         }
         subpath[c++]='/';
      }
      subpath[c]= 0;
      if (doList) {ui->dirLabel->setText(subpath);}
   }
   else { if (doList) {ui->dirLabel->setText(dstr);} }
   // Now load it to dirbuf
   LoadDirBuf(&DirCurrentStartCluster, dirbuf);
   LoadSubDir(false, doList);
}
void GemdDlg::OpenFATSubDir(unsigned char* DirBuffer, int EntryPos, bool doList)
{
   if (EntryPos >= 0) {
      int o = DirBuffer[EntryPos+11] ;
      if ((o & 16) == 16 ) {     // If not SubDir break
         //SetCursor( jc) ;
         PartSubDirLevel++ ;
         DirCurrentStartCluster = DirBuffer[EntryPos+26]+256*DirBuffer[EntryPos+27] ;
         //Get dir name:
         for (int o=0; o<11; o++) {
            // store SUBdir name
            subnam[o][PartSubDirLevel] = DirBuffer[EntryPos+o];
         }
         subdirh(doList);
      }
   }
}

void GemdDlg::FATDirUp(unsigned char* DirBuffer, char* NameBuffer, bool doList)
{
   //are we in subdir? :
   if (dirbuf[0] == 0x2E) {
      //SetCursor( jc) ;
      DirCurrentStartCluster = DirBuffer[32+26]+256*dirbuf[32+27] ;
      if (DirCurrentStartCluster == 0 ) { loadroot(doList); }     //it is root then
      else {
         PartSubDirLevel--;
         if (NameBuffer != NULL){
            for (int o=0;o<11;o++) { NameBuffer[o] = subnam[o][PartSubDirLevel]; }
         }
         subdirh(doList) ;
      }
   }
}
void GemdDlg::EnterSubDir(unsigned char* DirBuffer, int EntryPos, bool MakeLocalDir, bool EnterLocalDir)
{
   int o;
   char DirName[13];

   pDirEntryPos[PartSubDirLevel] = EntryPos; //we need the position if we come back up
   GetFATFileName(DirBuffer, EntryPos, DirName);
   OpenFATSubDir(DirBuffer, EntryPos, false);
   if(MakeLocalDir){
      #ifdef WINDOWS
      o = mkdir(DirName);
      #else
      o = mkdir(DirName,0777);
      #endif
   }
   if (EnterLocalDir) {
      o = chdir(DirName);
   }
   if(o) {/* something to do here */}
   ui->dirLabel->setText(DirName);
   QCoreApplication::processEvents();
}

bool GemdDlg::EnterUpDir(unsigned char* DirBuffer, char* NameBuffer, unsigned int* UpEntryPos, bool ChangeLocalDir) //returns IsRoot
{
   if (PartSubDirLevel == 0) { //we're already on root level
      return true;
   }
   FATDirUp(DirBuffer, NameBuffer, false);
   if (NameBuffer != NULL) {
      ui->dirLabel->setText(NameBuffer);
      QCoreApplication::processEvents();
   }
   if(ChangeLocalDir){
      chdir("..");
   }
   *UpEntryPos = pDirEntryPos[PartSubDirLevel];
   return (PartSubDirLevel == 0);
}


void GemdDlg::on_setddP_clicked()
{
   fileName = QFileDialog::getExistingDirectory(this, tr("Select dest. dir:"), "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
   if( !fileName.isEmpty() )
   {
      strcpy(DestDir, (char*)fileName.toLatin1().data());
      chdir(DestDir);
   }
}

void GemdDlg::on_filesLB_doubleClicked(const QModelIndex &index)
{
   if (PartSubDirLevel > 0) {
       if (index.row() == 0) { //updir
           FATDirUp(dirbuf, NULL, true);
       } else {
           OpenFATSubDir(dirbuf, DirEntrySortPos[index.row()-1], true);
       }

   } else { OpenFATSubDir(dirbuf, DirEntrySortPos[index.row()], true); }
}

void GemdDlg::on_timeCB_clicked()
{
   if( ui->timeCB->isChecked() ) timestCur=1;
   else timestCur=0;
}

#ifdef WINDOWS
void SetLocalFileDateTime(int fdate, int ftime, HANDLE fHandle)
#else
void SetLocalFileDateTime(int fdate, int ftime, char* FileName)
#endif
{
    #ifdef WINDOWS
    FILETIME  fildt;
    #else
    struct utimbuf fildt;
    time_t tim ;
    struct tm tout;
    QString qhm;
    #endif

    #ifdef WINDOWS
    DosDateTimeToFileTime(fdate, ftime, &fildt);
    SetFileTime(fHandle, &fildt, &fildt, &fildt);
    #else
    tout.tm_sec  = (ftime & 0x1F) << 1;
    tout.tm_min  = (ftime>>5) & 0x3F ;
    tout.tm_hour = (ftime>>11) & 0x1F ;

    tout.tm_mday  = fdate & 0x1F ;
    tout.tm_mon   = ((fdate>>5) & 0x0F) - 1 ;
    tout.tm_year  = ((fdate>>9) & 0x7F) + 80 ;
    tout.tm_isdst = 0 ;
    //dirLabel->setText(asctime( &tout));
    tim = mktime( &tout) ;
    fildt.actime = tim;
    fildt.modtime = tim;
    if (utime(FileName, &fildt) != 0){
       int i=errno;

    }
    #endif
}

int GemdDlg::ExtractFile(unsigned char* DirBuffer, int EntryPos)
{
   bool doloop = false;
   int rc = 0;
   unsigned int StartCluster, NextCluster, FileSector;
   unsigned int FileLength, msapos = 0;
   unsigned char DataBuffer[0x10000];  // 128 sector
   int RemainingBytes;
   char name[13];

   //load file from drive/image:
   StartCluster = DirBuffer[EntryPos+26]+256*DirBuffer[EntryPos+27] ;
   FileLength = 16777216*DirBuffer[EntryPos+31]+65536*DirBuffer[EntryPos+30]+256*DirBuffer[EntryPos+29]+DirBuffer[EntryPos+28] ;
   RemainingBytes = FileLength;
   fdat = DirBuffer[EntryPos+24]+256*DirBuffer[EntryPos+25] ;
   ftim = DirBuffer[EntryPos+22]+256*DirBuffer[EntryPos+23] ;
   GetFATFileName(DirBuffer, EntryPos, name);
   // open save file:
   fhout = OpenFileX(name, "wb");
   if (fhout == FILE_OPEN_FAILED) {
      QString qhm;
      qhm.setNum(GetLastError());
      qhm.insert(0, "File open error. (");
      qhm.append(")");
      QMessageBox::critical(this, name, qhm, QMessageBox::Cancel, QMessageBox::Cancel);
      ui->errti->setText("Error!");
	  return 1;
   }
   long tosave = PartSectorSize*PartSectorsPerCluster ;
   // get sector(s) to load and next cluster
   do {
      FATlookUp(StartCluster, &NextCluster, &FileSector);
      ReadSectors(FileSector, PartSectorsPerCluster, DataBuffer);
      if ( RemainingBytes<(int)(PartSectorSize*PartSectorsPerCluster) ) { tosave = RemainingBytes; }
      ULONG written;
      if ((written = WriteToFile(DataBuffer, 1, tosave, fhout)) > 0) {
         msapos = msapos + written;
      }
      RemainingBytes = RemainingBytes-PartSectorSize*PartSectorsPerCluster;
      doloop = ( RemainingBytes >= 0 ) && ( NextCluster <= 0xfff0 );
      if (doloop) {StartCluster = NextCluster;}
   } while(doloop);
   if (msapos != FileLength ) {
      ui->errti->setText("error!");
      rc = 1;
   }
   else if (!timestCur) {
      SetLocalFileDateTime(fdat, ftim
                #ifdef WINDOWS
                , fhout
                #else
                , dstr
                #endif
                );
   }
   CloseFileX(fhout);
   return rc;
}


void GemdDlg::GetFATFileName(unsigned char* DirBuffer, int EntryPos, char* NameBuffer)
{
   unsigned int i, k;
   
   for(i=0;i<8;i++) {
      if (DirBuffer[EntryPos+i] == 32) break ;
      NameBuffer[i] = DirBuffer[EntryPos+i] ;
   }
   if (DirBuffer[EntryPos+8] != 32){
      NameBuffer[i] = '.' ;
	  i++ ;
	  for(k=8;k<11;k++) {
         if (DirBuffer[EntryPos+k] == 32) { break; }
         NameBuffer[i] = DirBuffer[EntryPos+k];
		 i++;
	  }
   }
   NameBuffer[i] = 0 ; // String term.
   PadTOStoLocal(NameBuffer);
   ui->exwht->setText(NameBuffer);
   //ui->errti->setText(" ");
   QCoreApplication::processEvents();
}
bool IsEmpty(unsigned char* DirBuffer, int EntryPos)
{
   unsigned char o;
   o = DirBuffer[EntryPos] ;
   if ( o == 0 ) { return true; }
   return false;
}
bool IsDeleted(unsigned char* DirBuffer, int EntryPos)
{
   unsigned char o;
   o = DirBuffer[EntryPos] ;
   if ( o == 0xE5 ) { return true; }
   return false;
}
bool IsSubDir(unsigned char* DirBuffer, int EntryPos)
{
   unsigned char o;
   o = DirBuffer[EntryPos+11] ;
   if ( (o & 0x10) == 16 ) { return true; }
   return false;
}
bool IsFile(unsigned char* DirBuffer, int EntryPos)
{
   unsigned char o;
   o = DirBuffer[EntryPos+11] ;
   if ((o & 0x18) == 0) { return true; }
   return false;
}
bool IsVolume(unsigned char* DirBuffer, int EntryPos)
{
   unsigned char o;
   o = DirBuffer[EntryPos+11] ;
   if ((o & 0x08) == 8 ) { return true; }
   return false;
}
unsigned int GetFATFileLength(unsigned char* DirBuffer, int EntryPos)
{
   unsigned int m;
   m =  DirBuffer[EntryPos+28];
   m += DirBuffer[EntryPos+29] << 8;
   m += DirBuffer[EntryPos+30] << 16;
   m += DirBuffer[EntryPos+31] << 24;
   return m;
}

void SetFATFileLength(unsigned char* DirBuffer, int EntryPos, unsigned int Length)
{
   DirBuffer[EntryPos+28] = Length;
   DirBuffer[EntryPos+29] = Length>>8;
   DirBuffer[EntryPos+30] = Length>>16;
   DirBuffer[EntryPos+31] = Length>>24;
}

void GemdDlg::SetFATFileDateTime(unsigned char* DirBuffer, int EntryPos, struct stat* FileParams)
{
   unsigned int n;
   time_t tim ;
   tm tout;
   #ifdef WINDOWS
   tm * trc;
   #endif

   time(&tim);
   //struct tm tout;
   #ifdef WINDOWS
   if (FileParams == NULL) { trc = localtime(&tim); }
   else { trc = localtime(&FileParams->st_mtime);}
   if (trc != NULL) {
       memcpy(&tout, trc, sizeof(tm));
   } else {
       char message[256];
       char name[13];
       GetFATFileName(DirBuffer, EntryPos, name);
       sprintf(message, "File %s : Creation date faulty!", name);
       QMessageBox::warning(this, "date / time error", message, QMessageBox::Cancel);
       tout.tm_sec = 0;
       tout.tm_min = 0;
       tout.tm_hour = 0;
       tout.tm_mday = 1;
       tout.tm_mon = 1;
       tout.tm_year = 85;
   }
   #else
   if (FileParams == NULL){ localtime_r( &tim, &tout); } // Current time for timestamp
   else { localtime_r( &FileParams->st_mtime, &tout);} // Filetime for timestamp
   #endif
   n = (tout.tm_sec/2)|(tout.tm_min<<5)|(tout.tm_hour<<11) ;
   fatTime = n ;
   DirBuffer[EntryPos+22] = n;
   DirBuffer[EntryPos+23] = n>>8;
   n = (tout.tm_mday)|((tout.tm_mon+1)<<5)|( (tout.tm_year-80)<<9) ;
   fatDate = n;
   DirBuffer[EntryPos+24] = n;
   DirBuffer[EntryPos+25] = n>>8;
}

void GetFATFileDateTime(unsigned char* DirBuffer, int EntryPos, tm* DateTime)
{
   unsigned int n;

   n = DirBuffer[EntryPos+22];
   n = n + (DirBuffer[EntryPos+23] << 8);
   fatTime = n ;
   DateTime->tm_hour =  n >> 11;
   DateTime->tm_min  = (n >> 5) & 0x003F;
   DateTime->tm_sec  = (n & 0x1F) << 1;
   //n = (tout.tm_sec/2)|(tout.tm_min<<5)|(tout.tm_hour<<11) ;

   n = DirBuffer[EntryPos+24];
   n = n + (DirBuffer[EntryPos+25] << 8);
   fatDate = n;
   DateTime->tm_year = (n >> 9) + 80;
   DateTime->tm_mon  = ((n >> 5) & 0x000F) - 1;
   DateTime->tm_mday =  n & 0x001F;
   //n = (tout.tm_mday)|((tout.tm_mon+1)<<5)|( (tout.tm_year-80)<<9) ;

}

void SortFATNames(unsigned char* DirBuffer, unsigned int* UnSortList, unsigned int* SortList, unsigned int* SortIndex)
{
   QStringList VolNameList;
   QStringList DirNameList;
   QStringList FileNameList;
   unsigned int i, k;

   i = 0;
   for (QStringList::iterator sl = DirRawFilelist.begin(); sl != DirRawFilelist.end(); ++sl) {
       QString CurrentItem = *sl;
       if( !IsEmpty(DirBuffer, UnSortList[i]) && !IsDeleted(DirBuffer, UnSortList[i])){
          if (IsVolume(DirBuffer, UnSortList[i])){
              VolNameList.append(CurrentItem);
          }
          if (IsSubDir(DirBuffer, UnSortList[i])){
              DirNameList.append(CurrentItem);
          }
          if (IsFile(DirBuffer, UnSortList[i])){
              FileNameList.append(CurrentItem);
          }
      }
      i++;
   }
   VolNameList.sort();
   DirNameList.sort();
   FileNameList.sort();
   k = 0;
   for (QStringList::iterator sl = VolNameList.begin(); sl != VolNameList.end(); ++sl) {
      QString SortItem = *sl;
      i = 0;
      for (QStringList::iterator pl = DirRawFilelist.begin(); pl != DirRawFilelist.end(); ++pl) {
         QString PrimItem = *pl;
         if (SortItem.compare(PrimItem, Qt::CaseInsensitive) == 0){
            SortList[k] = UnSortList[i];
            SortIndex[k] = i;
            k++;
         }
         i++;
      }
   }
   for (QStringList::iterator sl = DirNameList.begin(); sl != DirNameList.end(); ++sl) {
      QString SortItem = *sl;
      i = 0;
      for (QStringList::iterator pl = DirRawFilelist.begin(); pl != DirRawFilelist.end(); ++pl) {
         QString PrimItem = *pl;
         if (SortItem.compare(PrimItem, Qt::CaseInsensitive) == 0){
            SortList[k] = UnSortList[i];
            SortIndex[k] = i;
            k++;
         }
         i++;
      }
   }
   for (QStringList::iterator sl = FileNameList.begin(); sl != FileNameList.end(); ++sl) {
      QString SortItem = *sl;
      i = 0;
      for (QStringList::iterator pl = DirRawFilelist.begin(); pl != DirRawFilelist.end(); ++pl) {
         QString PrimItem = *pl;
         if (SortItem.compare(PrimItem, Qt::CaseInsensitive) == 0){
            SortList[k] = UnSortList[i];
            SortIndex[k] = i;
            k++;
         }
         i++;
      }
   }
   for(i=k; i<DIR_ENTRIES_MAX_CNT;i++){ SortList[i] = 0; }

}

void GemdDlg::EraseFile(unsigned char* DirBuffer, int EntryPos)
{
   unsigned int StartCluster, NextCluster, FileSector, FileLength, l;
   bool CheckFileLenght;

   if (DirBuffer[EntryPos] == 0) return;  // Skip empty
   if (DirBuffer[EntryPos] == 0xE5) return; // Skip deleted
   StartCluster = DirBuffer[EntryPos+26]+256*DirBuffer[EntryPos+27] ;
   // free clusters
   CheckFileLenght = IsFile(DirBuffer, EntryPos);
   if (CheckFileLenght) { FileLength = GetFATFileLength(DirBuffer, EntryPos); }
   else { FileLength = 1; }
   l = 0;
   do {
      FATlookUp(StartCluster, &NextCluster, &FileSector);
      FATfreeCluster(StartCluster);
      StartCluster = NextCluster;
      if (CheckFileLenght) { l = l + PartSectorsPerCluster * PartSectorSize; }
   } while((StartCluster <= 0xfff0) && (l < FileLength));
   DirBuffer[EntryPos] = 0xE5; // erase Dir entry
}


bool GemdDlg::AddDirTreeToCurrentDir(QString PathName, unsigned char* DirBuffer){

    int EntryPos, CurLocalDirLevel;

    //PartSubDirLevel

    int LocalDirLevel = PathName.count(QLatin1Char('/'));
    QFileInfo fi(PathName);
    QString fn = fi.fileName();
    EntryPos = AddSingleDirToCurrentDir(fn, DirBuffer);
    if (EntryPos == -1) {return false;}
    //Write Directory: (<-needed on each Dir change)
    WriteCurrentDirBuf();
    EnterSubDir(DirBuffer, EntryPos, false, false);
    CurLocalDirLevel = LocalDirLevel + 1;

    QDirIterator it(PathName, QDir::NoFilter, QDirIterator::Subdirectories);
    while (it.hasNext()) {
       QString qhm = it.next();
       QFileInfo fi = it.fileInfo();
       if (fi.fileName().compare(".") == 0) {continue;}
       if (fi.fileName().compare("..") == 0) {continue;}
       int NewLocalDirLevel = qhm.count(QLatin1Char('/'));
       while (NewLocalDirLevel < CurLocalDirLevel){
          WriteCurrentDirBuf();
          FATDirUp(DirBuffer, NULL, false);
          CurLocalDirLevel--;
       }
       if (fi.isDir()){
          EntryPos = AddSingleDirToCurrentDir(fi.fileName(), DirBuffer);
          if (EntryPos == -1) {break;}
          WriteCurrentDirBuf();
          EnterSubDir(DirBuffer, EntryPos, false, false);
          CurLocalDirLevel++;
       } else if (fi.isFile()){
          if (!AddFileToCurrentDir(qhm, DirBuffer)) { break; }
       }
    }
    while (LocalDirLevel < CurLocalDirLevel){
       WriteCurrentDirBuf();
       FATDirUp(DirBuffer, NULL, false);
       CurLocalDirLevel--;
    }
    return true;
}



int PadFileName(char *Instr, char* Outstr)
{
   int i, n, k, c;
   // Look where is .
   k = strlen(Instr);
   for (n=0; n<k; n++) {
      if ( Instr[n] == '.' ) break;
   }
   if (k>8) {
      if (n>8) return 0 ;
   }// Invalid filename
   strncpy(Outstr,Instr,n+1) ;
   // Padd name with  spaces by need
   for (c=n;c<8;c++){ Outstr[c]=' '; }
   // Extension if is
   for (i=8;n<k-1;i++) {Outstr[i]=Instr[n+1]; n++; }
   for (c=i;c<11;c++) Outstr[c]=' ';
   Outstr[c]= 0;
   for (c=0;c<12;c++) {// Make all capital
       if((Outstr[c]>96) && (Outstr[c]<123)) {Outstr[c] = Outstr[c] & 0xDF; }
   }
   PadLocalToTOS(Outstr); //pad ATARI special cases
   for (c=0;c<12;c++) {// remove garbage
       if(Outstr[c]>96) {Outstr[c] = '_'; }
   }
   return 1;
}

int MakeFATFileName(char* SrcFileName, char* DestFileName)
{
   int  length, c, k, o, i, p;
   char curzx[128];
   char filext[20];
   char fnamb[128];

   length = strlen(SrcFileName) ;
   // extract filename only:
   p = 0;
   for (c=length-1; c>0; c--){
      #ifdef WINDOWS
      if ( SrcFileName[c] == '\\' ){break;}
      #endif
      if ( SrcFileName[c] == '/' ) {break;}
   }
   c++ ;
   while (c<length)
   {
      curzx[p++] = SrcFileName[c++];
      curzx[p] = 0;
   }
   // Convert filename to 8.3 cap format
   length = strlen(curzx) ;
   o = 0;
   i = 0;
   for (c=0;c<length;c++){
      k=curzx[c];
      if (k==' ') k='_' ; // Space to under
      if ((!o) && (k=='.')) { o = c;} // get pos of first .
      if (o) { filext[i] = k; i++; } // build extension
      fnamb[c] = k ;
   }
   fnamb[c] = 0;
   filext[i]=0;
   // Limit filename base to 8 length
   if (o) {
      fnamb[o] = 0;
      if (o>8) fnamb[8] = 0 ; // max 8 char
      if (strlen(filext)>4) filext[4]=0;
      strcat(fnamb,filext);
   }
   else if (length>8) fnamb[8] = 0 ;
   //exwht->setText(fnamb);
   return(PadFileName(fnamb, DestFileName));
}

bool GetFirstFreeDirSlot(unsigned char* DirBuffer, unsigned int* NewEntryPos)
{
   unsigned short fx;

   if (PartSubDirLevel==0) fx = 0;
   else fx=64; // By subdir is 2 first slot occupied
   for ( *NewEntryPos = fx; *NewEntryPos<(DirCurrentLenght*PartSectorSize);*NewEntryPos=*NewEntryPos+32) {
      if (DirBuffer[*NewEntryPos]==0) return true;
   }
   //If not found 0 seek E5 for deleted slot:
   for ( *NewEntryPos = fx; *NewEntryPos<(DirCurrentLenght*PartSectorSize);*NewEntryPos=*NewEntryPos+32) {
      if (DirBuffer[*NewEntryPos]==0xE5) return true;
   }
   return false ;
}
int CheckNameExistsInDir(char* Name, unsigned char* DirBuffer)
{
   unsigned int n, p, fx;

   if (PartSubDirLevel==0) p = 0;
   else p = 64; // By subdir is 2 first slot occupied
   for ( fx = p; fx<(DirCurrentLenght*PartSectorSize);fx=fx+32) {
      for (n=0;n<11;n++) {
         if ( Name[n] != DirBuffer[fx+n]) break;
      }
      if (n==11) return 0; // Same name found
   }
   return 1; // Not found same name
}
unsigned long SeekFirstFreeCluster(void)
{
   unsigned int m, fx;

   for( m = 4; m < (2*PartTotalDataClusters+4);  m = m+2 ) {
      fx = PartFATbuffer[m]+256*PartFATbuffer[m+1] ;
      if ( fx == 0 ) break  ; // free cluster
   }
   if ( m ==(2*PartTotalDataClusters+4) ) return INVALID_VALUE ; // Not found
   return m/2 ; // This is first free cluster #
}

unsigned long SeekNextFreeCluster(unsigned long StartCluster)
{
   unsigned long m, fx, ne;

   ne = 2 * StartCluster + 2 ; // Twice cluster # and go ahead to next
   for( m = ne; m < (2*PartTotalDataClusters+4);  m = m+2 )
   {
      fx = PartFATbuffer[m]+256*PartFATbuffer[m+1] ;
      if ( fx == 0 ) break  ; // free cluster
   }
   if ( m==(2*PartTotalDataClusters+4) ) { m=m/2; return INVALID_VALUE; } // Not found
   return m/2 ; // This is first free cluster #
}

void MarkCluster(unsigned long Cluster, unsigned long PointsTo)
{
   Cluster = Cluster*2 ;
   PartFATbuffer[Cluster]= PointsTo;
   PartFATbuffer[Cluster+1]= PointsTo >> 8;
}

int GemdDlg::MakeSubF(unsigned int Clun) // Create start cluster of subdirectory
{
   int n;
   unsigned int FileSector;

   // Calculate logical sector by cluster #
   FileSector = PartHeadSectors+(Clun-2)*PartSectorsPerCluster ;
   // Create content:
   // First clear:
   for (UINT i=0; i<(PartSectorSize*PartSectorsPerCluster); i++) trasb[i]=0 ;
   trasb[0] = 0x2E ;
   for (UINT i =1; i<11;i++ ) trasb[i] = ' ' ;
   trasb[11] = 16 ; // SubDir flag
   n = fatTime ;
   trasb[22] = n;
   trasb[23] = n>>8;
   n = fatDate ;
   trasb[24] = n;
   trasb[25] = n>>8;
   n = Clun ;
   trasb[26] = n;
   trasb[27] = n>>8; // Start cluster of itself
   trasb[32] = 0x2E ;
   trasb[33] = 0x2E ;
   for (UINT n =34;n<32+11;n++ ) trasb[n] = ' ' ;
   trasb[43] = 16 ; // SubDir flag
   n = DirParentStartCluster; //!!!!!!! zero if parent is ROOT
   trasb[26+32] = n;
   trasb[27+32] = n>>8; // Start cluster of parent
   // Write it to drive or image:
   UINT result = WriteSectors( FileSector, PartSectorsPerCluster, trasb);
   /*
   if ( res != (PartSectorsPerCluster*512) )   {
      MessageBox(hDlgWnd, "Write error!", "Problem",
      MB_OK | MB_ICONSTOP);
   }
   */
   if ( result != (PartSectorsPerCluster*512)) { return 0;}
   return 1;
}
int GemdDlg::AddSingleDirToCurrentDir(QString FilePathName, unsigned char* DirBuffer){

    unsigned long m,n;
    char subdnam[13];
    char dstr[60];
    unsigned int FileSector, EntryPos;

    strcpy(subdnam,(char*)FilePathName.toLatin1().data());
    char res = PadFileName(subdnam, dstr) ;
    if (!res) {
       QMessageBox::critical(this, tr("Invalid filename!"), tr("Must have dot before ext."), QMessageBox::Cancel, QMessageBox::Cancel);
       return -1;
    }
    // Testing
    ui->exwht->setText(dstr);
    // On free cluster at least needed
    if ( GetFreeClusters() < 1 ) {
      QMessageBox::critical(this, tr("Partition full!"), tr("No place"), QMessageBox::Cancel, QMessageBox::Cancel);
      return -1;
    }
    res = CheckNameExistsInDir(dstr, DirBuffer) ;
    if (!res) {
       QMessageBox::critical(this, tr("Name exists!"), tr("Name already in Dir."), QMessageBox::Cancel, QMessageBox::Cancel);
       return -1;
    }
    // Now seek first free slot in DIR:
    if (!GetFirstFreeDirSlot(DirBuffer, &EntryPos)){
       QMessageBox::critical(this, tr("DIR full!"), tr("Error"), QMessageBox::Cancel, QMessageBox::Cancel);
       return -1;
    }
    // Copy name at EntryPos, add timestamp and directory flag
    for (n=0;n<11;n++) DirBuffer[EntryPos+n]=dstr[n];
    DirBuffer[EntryPos+11]=16; // DIR flag
    //Set start cluster #
    // Seek first free cluster
    m = SeekFirstFreeCluster(); //we have already checked - there are definitely enough free ones
    n = m;
    DirBuffer[EntryPos+26] = n;
    DirBuffer[EntryPos+27] = n>>8;
    credirClu[0] = m ; // store spread of created
    credirCnt = 1;
    // seek for next 15 free cluster:
    ULONG oldCluster = m;
    n = PartSectorSize>>9 ;
    while ( credirCnt<(32/(PartSectorsPerCluster*n)) ) {
       if ((m = SeekNextFreeCluster(m)) == INVALID_VALUE) break ;
       credirClu[credirCnt] = m ;
       // write into FAT
       MarkCluster(oldCluster, m);
       credirCnt++ ;
       oldCluster = m;
    }
    // mark as last:
    MarkCluster(oldCluster, LAST_CLUSTER) ;
    // Create and write empty SubDir:
    // first cluster of:
    MakeSubF(credirClu[0]);
    // First clear:
    for (UINT n=0;n<(PartSectorSize*PartSectorsPerCluster);n++){ trasb[n]=0; }
    // clear following clusters of DIR:
    for (UINT i=1;i<credirCnt;i++) {
       // Calculate logical sector by cluster #
       FileSector = PartHeadSectors+(credirClu[i]-2)*PartSectorsPerCluster ;
       res = WriteSectors( FileSector, PartSectorsPerCluster, trasb);
    }
    return EntryPos;
}



bool GemdDlg::AddFileToCurrentDir(QString FilePathName, unsigned char* DirBuffer)
{
   char   FATFileName[256];
   char   LocalFileName[256];
   struct stat fparms ;
   unsigned int k, FileByteLength;
   unsigned int n, FileClusterCount, EntryPos, FileSector, CurrentFileCluster, PreviousFileCluster;
   FILE *inFile;

   strcpy(LocalFileName, (char*)FilePathName.toLatin1().data());   // Filename with path?
   MakeFATFileName(LocalFileName, FATFileName);
   ui->exwht->setText(FATFileName);   // show corrected filename
   QCoreApplication::processEvents();
   //sleep(1); //testing
   // Open file and get it's length:
   // Ignore directories:
   stat( LocalFileName , &fparms );
   if (S_IFDIR & fparms.st_mode ) { return(true); } //Frank !!!!!!!!!
   inFile = fopen(LocalFileName, "rb");
   if (inFile == NULL) {
      QMessageBox::critical(this, tr("File open error"), FilePathName, QMessageBox::Cancel, QMessageBox::Cancel);
      return(false);
   }
   //get filelength :
   fseek(inFile, 0, SEEK_END) ;
   FileByteLength = ftell(inFile); //Filelength got
   fseek(inFile,0,SEEK_SET); // Back to filestart dude!
   // Get needed cluster count:
   n = PartSectorsPerCluster*PartSectorSize;
   FileClusterCount = FileByteLength/n ;
   if ( FileByteLength % n) FileClusterCount++ ;
   // Check is there enough space:
   if ( GetFreeClusters() < FileClusterCount ) {
      QMessageBox::critical(this, tr("No place for file"), FilePathName, QMessageBox::Cancel, QMessageBox::Cancel);
      fclose(inFile);
      return(false);
   }
   // Is name already in cur DIR?
   res = CheckNameExistsInDir(FATFileName, dirbuf) ;
   if (!res) {
      QMessageBox::critical(this, tr("Name exists!"), "Name already in Dir.", QMessageBox::Cancel, QMessageBox::Cancel);
      fclose(inFile);
      return(false);
   }
   // Now seek first free slot in DIR:
   if (!GetFirstFreeDirSlot(DirBuffer, &EntryPos)) {
      QMessageBox::critical(this, tr("Directory full!"), "Error", QMessageBox::Cancel, QMessageBox::Cancel);
      fclose(inFile);
      return(false);
   }
   // Copy name at EntryPos, add timestamp and directory flag
   for (n=0;n<11;n++)
       DirBuffer[EntryPos+n]=FATFileName[n];
   //Enter filelength in filerecord:
   SetFATFileLength(DirBuffer, EntryPos, FileByteLength);
   SetFATFileDateTime(DirBuffer, EntryPos, (timestCur) ? NULL : &fparms);
   // Set start cluster #
   // Seek first free cluster
   CurrentFileCluster = SeekFirstFreeCluster(); //we have already checked - there are definitely enough free ones
   n = CurrentFileCluster;
   DirBuffer[EntryPos+26] = n;    //write 1st FileCluster to DirEntry
   DirBuffer[EntryPos+27] = n>>8;
   //Copy FileSectors in order:
   if ( (FileByteLength)<(PartSectorsPerCluster*PartSectorSize) )  {
      k = FileByteLength ;
      // First clear:
      for ( UINT n=0;n<(PartSectorSize*PartSectorsPerCluster);n++) trasb[n]=0 ; // clear trasb for have zeros after file end
   }
   else { k =  PartSectorsPerCluster*PartSectorSize; }
   res=fread(trasb,1,k,inFile);
   FileSector = PartHeadSectors+(CurrentFileCluster-2) * PartSectorsPerCluster;
   WriteSectors(FileSector, PartSectorsPerCluster, trasb);

   // if only 1 cluster mark it as last:
   if (FileClusterCount==1) {
      MarkCluster(CurrentFileCluster, LAST_CLUSTER);
   }
   else {
      unsigned int msapos = PartSectorsPerCluster*PartSectorSize;
      int o = 0 ; //flag
      for (UINT n=1; n<FileClusterCount; n++)
      {
         if ( (FileByteLength-msapos)<(PartSectorsPerCluster*PartSectorSize) ) {
            k = FileByteLength-msapos ;
            for (UINT i=0;i<(PartSectorSize*PartSectorsPerCluster);i++) { trasb[i]=0; } // clear trasb for have zeros after file end
            o = 1; // flag for last cluster
         }
         else { k = PartSectorsPerCluster*PartSectorSize; }
         res=fread(trasb,1,k,inFile);
         PreviousFileCluster = CurrentFileCluster;
         if ((CurrentFileCluster = SeekNextFreeCluster(CurrentFileCluster)) == INVALID_VALUE)
         {
             MarkCluster(PreviousFileCluster, LAST_CLUSTER) ; break ;
         } // Error message?
         // write into FAT
         MarkCluster(PreviousFileCluster, CurrentFileCluster);
         if ( o ) MarkCluster(CurrentFileCluster, LAST_CLUSTER) ;
         //PreviousFileCluster = m;
         FileSector = PartHeadSectors+(CurrentFileCluster - 2)*PartSectorsPerCluster ;
         WriteSectors( FileSector, PartSectorsPerCluster, trasb);
         msapos = msapos+PartSectorSize*PartSectorsPerCluster ;
      }
   }
   fclose(inFile);
   // Write parent DIR with new entry to drive/image:
   return(true);
}

void GemdDlg::on_opdirP_clicked()
{

   QString dir = QFileDialog::getExistingDirectory(this, tr("Select directory to write to device"), "",
                                                QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
   if(!dir.isEmpty()){

       AddDirTreeToCurrentDir(dir, dirbuf);

       WriteCurrentDirBuf();
       //Write FAT back:
       WriteSectors( PartReservedSectors, PartSectorsPerFAT, PartFATbuffer);
       //Second FAT
       WriteSectors( PartReservedSectors+PartSectorsPerFAT, PartSectorsPerFAT, PartFATbuffer);
       // Refresh Used/free indicator:
       ShowPartitionUsage() ;
       // Need to refresh listbox content
       LoadSubDir((PartSubDirLevel == 0), true) ;

       setCursor(QCursor(Qt::ArrowCursor));
   } // if Seldfiles end


/*
   QFileDialog DirSelector;
   DirSelector.setFileMode(QFileDialog::DirectoryOnly);
   DirSelector.setOption(QFileDialog::DontUseNativeDialog,true);
   QListView *l = DirSelector.findChild<QListView*>("listView");
   if (l) {
      l->setSelectionMode(QAbstractItemView::MultiSelection);
   }
   QTreeView *t = DirSelector.findChild<QTreeView*>();
   if (t) {
      t->setSelectionMode(QAbstractItemView::MultiSelection);
   }
   DirSelector.setWindowIcon(QIcon("://Icons/atarifolder.png"));
   DirSelector.setWindowTitle("Select diretories to write to device");

   if(DirSelector.exec() == 0){ return; }
   if(!DirSelector.selectedFiles().isEmpty())  {
      setCursor(QCursor(Qt::WaitCursor));

      QCoreApplication::processEvents(); // refresh dialog content
      setCursor(QCursor(Qt::WaitCursor));

      int i= DirSelector.selectedFiles().count();
      for (int k=0; k<i; k++){
         if (!AddDirTreeToCurrentDir(DirSelector.selectedFiles().value(k), dirbuf)) { break; }
      }
      WriteCurrentDirBuf();
      //Write FAT back:
      WriteSectors( PartReservedSectors, PartSectorsPerFAT, PartFATbuffer);
      //Second FAT
      WriteSectors( PartReservedSectors+PartSectorsPerFAT, PartSectorsPerFAT, PartFATbuffer);
      // Refresh Used/free indicator:
      ShowPartitionUsage() ;
      // Need to refresh listbox content
      LoadSubDir((PartSubDirLevel == 0), true) ;

      setCursor(QCursor(Qt::ArrowCursor));
   } // if Seldfiles end
   */
}

void GemdDlg::on_DeleteFiles_clicked()
{
   unsigned int CurrentDirEntryPos, button;
   int  DirLevel, listOffset, FileRemainsLevel;
   char message[256];
   bool Abort = false;
   bool YesToAll = false;

   if (ui->filesLB->currentRow() < 0) { return; }
   if ( QMessageBox::warning(this, "Deleting files!",
                             "You are about to delete files and / or directories.\nThey can not be restored!\n\nAre you sure?",
                             QMessageBox::Yes | QMessageBox::Cancel) == QMessageBox::Cancel) { return; }

   //QMessageBox::critical(this, "Deleting!", "Deleting", QMessageBox::Cancel);

   listOffset = (PartSubDirLevel > 0) ? 1 : 0;
   setCursor(QCursor(Qt::WaitCursor));
   DirLevel   = 0;
   FileRemainsLevel = 0;
   pDirEntryPos[0] = 0;
   // Get count of items in listbox
   int litc = ui->filesLB->count()-listOffset;
   for (int p=0;p<litc;p++) { //step through listed files
      // see is item selected:
      if ( ui->filesLB->item(p+listOffset)->isSelected()) { // care for selected only
         //ui->filesLB->item(p+listOffset)->setSelected(false);
         dpos = DirEntrySortPos[p];// Position in DIR
         if (IsVolume(dirbuf, dpos)) { continue; }    //avoid VOL extraction
         GetFATFileName(dirbuf, dpos, dstr);
         // Recursive subdir extraction Open SubDir, show files, and extract
         // Need to store level, parent & pos in parent
         if (IsSubDir(dirbuf, dpos)) {
            if (!YesToAll) {
               sprintf(message, "Delete subdirectory %s including all files below?", dstr);
               button = QMessageBox::warning(this, dstr, message, QMessageBox::Yes | QMessageBox::No | QMessageBox::YesToAll | QMessageBox::Abort);
               if (button ==  QMessageBox::No){ continue; }//next entry
               if ((Abort = (button ==  QMessageBox::Abort)) == true){ break; }
               YesToAll = (button ==  QMessageBox::YesToAll);
            }
            // Store pos in parent, here by selection number
            EnterSubDir(dirbuf, dpos, false, false);
            DirLevel++;
            CurrentDirEntryPos = 64; // 1st 2 entries of sub dir are '.' and '..'
            do{
               bool intoNextSubDir = false;
               for (UINT n=CurrentDirEntryPos; n<PartSectorSize*DirCurrentLenght; n=n+32) {
                  if (IsEmpty(dirbuf, n)) { break; }
                  if (IsDeleted(dirbuf, n)) { continue; }
                  GetFATFileName(dirbuf, n, dstr);
                  if (IsFile(dirbuf, n)) { //no subdir, it's a file
                     if (!YesToAll) {
                        sprintf(message, "Delete file %s?", dstr);
                        button = QMessageBox::warning(this, dstr, message, QMessageBox::Yes | QMessageBox::No | QMessageBox::YesToAll | QMessageBox::Abort);
                        if (button ==  QMessageBox::No){ FileRemainsLevel = DirLevel; continue; }//next entry
                        if ((Abort = (button ==  QMessageBox::Abort)) == true){ break; }
                        YesToAll = (button ==  QMessageBox::YesToAll);
                     }
                     EraseFile(dirbuf, n);
                  }else if (IsSubDir(dirbuf, n)) {
                     // its a subdir -> store pos in dir, leave n loop
                     if (!YesToAll) {
                        sprintf(message, "Delete subdirectory %s including all files below?", dstr);
                        button = QMessageBox::warning(this, dstr, message, QMessageBox::Yes | QMessageBox::No | QMessageBox::YesToAll | QMessageBox::Abort);
                        if (button ==  QMessageBox::No){ FileRemainsLevel = DirLevel; continue; }//next entry
                        if ((Abort = (button ==  QMessageBox::Abort)) == true){ break; }
                        YesToAll = (button ==  QMessageBox::YesToAll);
                     }
                     EnterSubDir(dirbuf, n, false, false);
                     DirLevel++;
                     intoNextSubDir = true;
                     break;
                  }
               }  //n loop end
               if(intoNextSubDir){ continue; }
               //we are though
               if (FileRemainsLevel == DirLevel){ //there wss a file left in SubDir
                  WriteCurrentDirBuf(); //rewrite Dir
               }
               //dir up
               EnterUpDir(dirbuf, dstr, &CurrentDirEntryPos, true);
               DirLevel--;
               //do not delete dirs not empty
               if (FileRemainsLevel <= DirLevel) {
                  EraseFile(dirbuf, CurrentDirEntryPos); //just mark Clusters as free
               }
               if (FileRemainsLevel > DirLevel) {FileRemainsLevel = DirLevel; }
               if (DirLevel == 0) { //we're on topmost level
                  break;
               }
               CurrentDirEntryPos += 32; // point to next entry
               //are we in subdir? :
               if (dirbuf[0] != 0x2E) { QMessageBox::critical(this, "Curious problem!", "Bounced to bottom of root directory.\n This should never happen!", QMessageBox::Cancel); return; } //If it's not 'dot' entry then we're through with root DIR. If, it's the end of a SUBDIR
            }while( PartSubDirLevel > 0);
            if (Abort) { break; }
            continue; //next main dir entry
         }
         if (Abort) { break; }
         if (!YesToAll) {
            sprintf(message, "Delete file %s?", dstr);
            button = QMessageBox::warning(this, dstr, message, QMessageBox::Yes | QMessageBox::No | QMessageBox::YesToAll | QMessageBox::Abort);
            if (button ==  QMessageBox::No){ FileRemainsLevel = DirLevel; continue; }//next entry
            if ((Abort = (button ==  QMessageBox::Abort)) == true){ break; }
            YesToAll = (button ==  QMessageBox::YesToAll);
         }
         EraseFile(dirbuf, dpos);
      }
      if (Abort) { break; }
   }
   if (!Abort) {
      WriteCurrentDirBuf(); //rewrite Dir
      //Write FAT back:
      WriteSectors( PartReservedSectors, PartSectorsPerFAT, PartFATbuffer);
      //Second FAT
      WriteSectors( PartReservedSectors+PartSectorsPerFAT, PartSectorsPerFAT, PartFATbuffer);
      // Refresh Used/free indicator:
   }
   ShowPartitionUsage() ;
   // Need to refresh listbox content
   LoadSubDir((PartSubDirLevel == 0), true) ;

   setCursor(QCursor(Qt::ArrowCursor));
}

void GemdDlg::on_ExtractFiles_clicked()
{
   unsigned int CurrentDirEntryPos;
   //int DirLevel;
   int listOffset;

   #ifdef FRANKS_DEBUG
   #ifdef WINDOWS
   strcpy(DestDir, "D:\\Projekte\\tools\\build-DrImg\\testoutput");
   #else
   strcpy(DestDir, "/home/frank/Projekte/ATARI/DrImg/testoutput");
   #endif
   if (chdir(DestDir)) {}
   #endif
   if(DestDir[0] == '\0'){
      on_setddP_clicked();
   }

   if (ui->filesLB->currentRow() < 0) { return; }
   listOffset = (PartSubDirLevel > 0) ? 1 : 0;
   setCursor(QCursor(Qt::WaitCursor));
   DirLevel   = 0 ;
   pDirEntryPos[0] = 0;
   // Get count of items in listbox
   int litc = ui->filesLB->count()-listOffset;
   for (int p=0;p<litc;p++) { //step through listed files
      // see is item selected:
      if ( ui->filesLB->item(p+listOffset)->isSelected()) { // care for selected only
         //ui->filesLB->item(p+listOffset)->setSelected(false);
         dpos = DirEntrySortPos[p];// Position in DIR
         if (IsVolume(dirbuf, dpos)) { continue; }  //avoid VOL extraction
         // Recursive subdir extraction Open SubDir, show files, and extract
         if (IsSubDir(dirbuf, dpos)) {
            EnterSubDir(dirbuf, dpos, true, true);
            DirLevel++;
            CurrentDirEntryPos = 64; // 1st 2 entries of sub dir are '.' and '..'
            do{
               bool intoNextSubDir = false;
               for (UINT n=CurrentDirEntryPos; n<PartSectorSize*DirCurrentLenght; n=n+32) {
                  if (IsEmpty(dirbuf, n)) break; //empty entry
                  if (IsDeleted(dirbuf, n)) continue; //deleted entry
                  GetFATFileName(dirbuf, n, dstr);
                  if (IsFile(dirbuf, n)) { //no subdir, it's a file
                     ExtractFile(dirbuf, n);
                  }else if (IsSubDir(dirbuf, n)) {
                     // its a subdir -> store pos in dir, leave n loop
                     EnterSubDir(dirbuf, n, true, true);
                     DirLevel++;
                     intoNextSubDir = true;
                     break;
                  }
               }  //n loop end
               if(intoNextSubDir){ intoNextSubDir = false; continue; }
               //we are though, dir up
               EnterUpDir(dirbuf, dstr, &CurrentDirEntryPos, true);
               DirLevel--;
               if (DirLevel == 0) { //we're on topmost level
                  break;
               }
               CurrentDirEntryPos += 32; // point to next entry
               //are we in subdir? :
               if (dirbuf[0] != 0x2E) { QMessageBox::critical(this, "Curious problem!", "Bounced to bottom of root directory.\n This should never happen!", QMessageBox::Cancel); return; } //If it's not 'dot' entry then we're through with root DIR. If, it's the end of a SUBDIR
            }while( PartSubDirLevel > 0);
            continue; //next main dir entry
         }
         ExtractFile(dirbuf, dpos);
      } // if selected end
   }  // p loop end
   setCursor(QCursor(Qt::ArrowCursor));
}

void GemdDlg::on_AddFiles_clicked()
{
   // Multiple file open dialQFileDialogog:
   QFileDialog FileSelector;
   FileSelector.setWindowIcon(QIcon("://Icons/atarifolder.png"));
   QStringList SeldFiles = FileSelector.getOpenFileNames(this, tr("Select files to write to device"), NULL, "All files(*.*);;ST PRG, TOS, TTP, APP files (*.prg *.tos *.ttp *.app);;Binary files(*.bin)");
   if(!SeldFiles.isEmpty())  {
      for (QStringList::Iterator iq = SeldFiles.begin(); iq != SeldFiles.end(); ++iq)
      {
         if (!AddFileToCurrentDir(*iq, dirbuf)) { break; }
      } // iq for loop end
      //Write Directory:
      WriteCurrentDirBuf();
      //Finally write FAT back:
      WriteSectors( PartReservedSectors, PartSectorsPerFAT, PartFATbuffer);
      //Second FAT
      WriteSectors( PartReservedSectors+PartSectorsPerFAT, PartSectorsPerFAT, PartFATbuffer);
      // Refresh Used/free indicator:
      ShowPartitionUsage() ;
      // Need to refresh listbox content
      LoadSubDir((PartSubDirLevel == 0), true) ;
   } // if Seldfiles end
   setCursor(QCursor(Qt::ArrowCursor))    ;
}

void GemdDlg::on_newfP_clicked()
{
   QString litet = ui->dirED->text();
   //GetDlgItemText(hDlgWnd, GemCreD , szText , (LPARAM) 64);
   if ( litet.length() == 0 ){
      QMessageBox::critical(this, tr("Invalid filename!"), tr("Enter something"), QMessageBox::Cancel, QMessageBox::Cancel);
      return;
   }
   // Look is name base 8 space:
   int c = 0;
   for (int n=0;n<8;n++) {
      if (litet[n]==' ') c++ ;
   }
   if (c==8) {
      QMessageBox::critical(this, tr("Invalid filename!"), tr("All the spaces..."), QMessageBox::Cancel, QMessageBox::Cancel);
      return;
   }
   int EntryPos = AddSingleDirToCurrentDir(litet, dirbuf);
   // Add current timestamp:
   SetFATFileDateTime(dirbuf, EntryPos, NULL);

   //Write Directory:
   WriteCurrentDirBuf();
   //Write FAT back:
   WriteSectors( PartReservedSectors, PartSectorsPerFAT, PartFATbuffer);
   //Second FAT
   WriteSectors( PartReservedSectors+PartSectorsPerFAT, PartSectorsPerFAT, PartFATbuffer);
   // Refresh Used/free indicator:
   ShowPartitionUsage() ;
   // Need to refresh listbox content
   LoadSubDir((PartSubDirLevel == 0), true) ;
}


void GemdDlg::on_desallP_clicked()
{
   ui->filesLB->clearSelection();
}

void GemdDlg::on_quitP_clicked()
{
   GemdDlg::close() ;
}

void GemdDlg::closeEvent (QCloseEvent *event)
{
   if(event){}
   if (fhdl != FILE_OPEN_FAILED){
      CloseFileX(fhdl);
   }
}
