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
extern FILE *finp, *fout, *ftout, *flout ;
extern ULONG filelen, u ;
extern ULONG ChaSecCnt ;

extern int selected;
extern int c,k,i, o, p, s, fscanp ;
extern char detDev[9][16] ;
extern char physd[9];
extern char loadedF[256] ;
extern int form;
extern int PartSectorSizee, clust ;
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

extern FILE *finp, *fout, *ftout, *flout ;
extern unsigned char bufr[640];
extern unsigned char  buf2[524];
extern unsigned char  bufsw[524];
extern QString litet;


int drih;
ULONG epos, eposIn, PartStartPosition, stfilelen, copied ;
ULONG Gpartp[24] ; // GemDos part postions
ULONG Gparsiz[24] ; // GemDos part sizes
ULONG  PartTotalSectors ,PartTotalDataClusters ;
ULONG  dirflen, oldclun ;
int PartSubDirLevel, reculev, selcnt, selP, j, d, e ;
int fdat, ftim ;
unsigned int oe, zz, staclu;
unsigned int dpos, fatTime, fatDate, PartSectorSize ;
unsigned int PartReservedSectors ; // Used clusters, free clusters of partition
UINT PartSectorsPerFAT , PartRootDirSectors, PartSectorsPerCluster, PartHeadSectors,  PartFirstRootDirSector, ClunOfParent  ;
long tosave;

unsigned int  DirCurrentClustes[98] ; // For storing DIR cluster -> Points To Sector
unsigned int  DirCurrentLenght;
unsigned int  DirCurrentClusterCnt;

unsigned int  dirpos[512] ;
unsigned int  pdirpos[8] ; // Position in parent dir, by recursive extraction
unsigned int  credirClu[16] ; // Created DIR cluster spread store
unsigned int  credirCnt;
unsigned char dirbuf[512*100];
unsigned char PartFATbuffer[132000] ;
unsigned char trasb[65536] ;  // 128 sector
char subnam[12][8] ;
char subdnam[13] ;
char subpath[256];
char DestDir[256];
char res ;
bool timestCur=0;
const unsigned long INVALID_VALUE = 0xFFFFFFFF;
const unsigned long LAST_CLUSTER  = 0xFFFF;

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
    ui->opdirP->setIcon(QIcon(":/Icons/OpenDir.png"));
    ui->quitP->setIcon(QIcon(":/Icons/Quit.png"));
    ui->setddP->setIcon(QIcon(":/Icons/DestDir.png"));
    ui->newfP->setIcon(QIcon(":/Icons/OpenFolder.png"));
    OpenDialog();
}

GemdDlg::~GemdDlg()
{
    delete ui;
}

// This is because of function name 'close' conflict!!!!!
int fileopen(const char* fname, const ULONG flags)
{
    int fd;
    fd = open(fname, flags);
    #ifdef WINDOWS
    _setmode(fd, _O_BINARY);
    #endif
    return fd;
}
int fileclose(int fd)
{
   return close(fd);
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

   // Open file or physical drive:
   if (selected<16){
      for (k=0;k<9;k++) { physd[k] = detDev[k][selected]; }
      if ((ov2ro)  && ( SecCnt>2097152 )) {
         drih = fileopen(physd, O_RDONLY );
      }
      else{
         drih = fileopen(physd, O_RDWR  );
      }
   }
   else{
      drih = fileopen(loadedF, O_RDWR  );
   }
   if (drih <0) {
      QMessageBox::critical(this, "Error", "Drive/file open error ", QMessageBox::Cancel, QMessageBox::Cancel);
      return; //gemdpex
   }
   // Load bootsector
   read(drih, bufr, 512);
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
      for (i=0x1BE;i<0x1EF;i=i+16) { // Four primary slots
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
               lseek(drih, epos*512, 0);
               read(drih, buf2, 512);
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
         for (n=0;n<512;n=n+2)
         { k=bufr[n] ; bufr[n]=bufr[n+1];bufr[n+1]=k; }
      }
      p = 0 ; // Partition counter (index)
      for (i=0x1C6;i<0x1F7;i=i+12) { // Four primary slots
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
gemsfp1:
            lseek(drih, epos*512, 0 );
            read(drih, buf2, 512);
            if (Swf) {
               // Swap low/high bytes:
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
               if ( p> 24 ) { break; }
               // Look for further Extended slots:
               if ((buf2[0x1D3] == 'X') && (buf2[0x1D4] == 'G') && (buf2[0x1D5] == 'M')) {
                  epos = eposIn+buf2[0x1D9]+buf2[0x1D8]*256 + buf2[0x1D7]*65536 +buf2[0x1D6]*16777216 ;
                  goto gemsfp1 ;
               }
            }
         }
      } // i loop end
   } // else end
}

void GemdDlg::on_partLB_clicked(const QModelIndex &index)
{
   unsigned int  n, m;
   unsigned char k;

   selP = index.row();
   //Load bootsector of partition:
   PartStartPosition =  Gpartp[selP]*512 ;
   lseek(drih, PartStartPosition, 0 );
   read(drih, buf2, 512);
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

   qhm.setNum( Gpartp[selP] );
   qhm.append(", ");
   QString num;
   if ( Filesys == 13) {
      num.setNum( PartTotalSectors );
   }
   else {
       num.setNum( Gparsiz[selP] );
   }
   qhm.append(num);
   ui->partinfLab->setText(qhm);
   // Load FAT #1
   lseek(drih, PartStartPosition+PartSectorSize*PartReservedSectors, 0) ;
   read(drih, PartFATbuffer, PartSectorsPerFAT*PartSectorSize);
   if (Swf)
   {   // Swap low/high bytes:
       for (m=0;m<PartSectorsPerFAT*PartSectorSize;m=m+2){
          k=PartFATbuffer[m] ; PartFATbuffer[m]=PartFATbuffer[m+1];PartFATbuffer[m+1]=k;
       }
   }
   // Testing:
   /*
   flout = fopen("TheFat.bin","wb");
   fwrite(PartFATbuffer,1,PartSectorsPerFAT*512 ,flout);
   fclose(flout);
   */
   // Load root DIR

   loadroot() ;
}

void GemdDlg::loadroot()
{
   lseek(drih, PartStartPosition+PartFirstRootDirSector*PartSectorSize, 0 );
   read(drih, dirbuf, PartRootDirSectors*PartSectorSize);
   if (Swf)
   {   // Swap low/high bytes:
      for (unsigned long m=0;m<PartRootDirSectors*PartSectorSize;m=m+2){
         k=dirbuf[m] ; dirbuf[m]=dirbuf[m+1];dirbuf[m+1]=k;
      }
   }
   // List files from ROOT dir:
   ui->dirLabel->setText("ROOT");
   //SetDlgItemText(hDlgWnd,CurDir,"ROOT");
   DirCurrentLenght = PartRootDirSectors ;
   PartSubDirLevel = 0;
   ClunOfParent = 0 ;
   DirCurrentLenght = PartRootDirSectors; // Current DIR length in sectors
   LoadSubDir(true, true);
   ShowPartitionUsage() ;
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

void   GemdDlg::opensubd(int index)
{
   if (index >= 0) {
      dpos = dirpos[index] ; // Position in DIR, only first selection
      o = dirbuf[dpos+11] ;
      if ((o & 16) == 16 ) {     // If not SubDir break
         //SetCursor( jc) ;
         PartSubDirLevel++ ;
         staclu = dirbuf[dpos+26]+256*dirbuf[dpos+27] ;
         //Get dir name:
         for (int o=0; o<11; o++) {
            dstr[o] = dirbuf[dpos+o] ;
            // store SUBdir name
            subnam[o][PartSubDirLevel] = dirbuf[dpos+o];
         }
      subdirh() ;
      }
   }  // perform only for first selected item!!!
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

   lseek(drih, PartStartPosition+StartSector*PartSectorSize, 0 );
   if (Swf) { j = Count*PartSectorSize/512 ;
      for (e=0;e<j;e++)
      { read(drih, bufsw, 512);
         for (d=0;d<512;d=d+2) { Buffer[d+e*512]=bufsw[d+1] ; Buffer[d+1+e*512]=bufsw[d]; }
      }
   }
   else {
      read(drih, Buffer, Count*PartSectorSize);
   }
   return 1;
}
ULONG WriteSectors( int StartSector, int Count, unsigned char *Buffer)
{
    lseek(drih, PartStartPosition+StartSector*PartSectorSize, 0 );
    if (Swf) {
        j = Count*PartSectorSize/512;
        u = 0;
        for (e=0;e<j;e++) {
           for (d=0;d<512;d=d+2) {
               bufsw[d+1]=Buffer[d+e*512] ;
               bufsw[d]=Buffer[d+1+e*512];
           }
          u = u+ write(drih, bufsw, 512);
        }
        return u ;
    }
    return  write(drih, Buffer, Count*PartSectorSize);
}

void GemdDlg::subdirh()
{
   dstr[11] = 0 ;
   ClunOfParent = staclu ; // Keep this value for SubDIR creation
   if ( PartSubDirLevel ) {
      int spafl;
      subpath[0] = '/';
      c=1;
      for(o=0;o<PartSubDirLevel;o++){
         spafl = 0;
         for(i=0;i<11;i++) {
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
      //strcat(subpath,dstr);
      ui->dirLabel->setText(subpath);
   }
   else { ui->dirLabel->setText(dstr); }
   //SetDlgItemText(hDlgWnd,CurDir,dstr);
   // Now load it to dirbuf
   LoadDirBuf(&staclu, dirbuf);
   //subdirl:
   LoadSubDir(false, true);
   //subdirhend: ; // Here some error message!!! TODO
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

int GemdDlg::LoadEntryName(unsigned char* DirBuffer, int EntryPos, bool IsRoot, bool doList)
{
   if (DirBuffer[EntryPos] == 0) return 1 ;  // Skip empty
   if (DirBuffer[EntryPos] == 0xE5) return 2 ; // Skip deleted
   if ((!IsRoot) && (DirBuffer[EntryPos] == 0x2E)) return 2 ; // Skip subdir marks
   GenerateFileName(DirBuffer, EntryPos, &qhm);
   // Put filename in list
   if (doList) ui->filesLB->addItem(qhm);
   return 0 ;
}

void GemdDlg::LoadSubDir(bool IsRoot, bool doList)
{
   unsigned int direc;

   if (doList){
       ui->filesLB->clear();
       if (!IsRoot) ui->filesLB->addItem("..");
   }
   direc = 0 ;
   for (UINT n=0;n<DirCurrentLenght*PartSectorSize; n=n+32) {
      int rc = LoadEntryName(dirbuf, n, IsRoot, doList);
      if (rc == 1) break;
      if (rc == 2) continue;
      dirpos[direc] = n ; // Store pos (bytewise) in DIR
      direc++ ;
   }
   QCoreApplication::processEvents();
}


void GemdDlg::DirUp()
{
   //are we in subdir? :
   if (dirbuf[0] == 0x2E) {
      //SetCursor( jc) ;
      staclu = dirbuf[32+26]+256*dirbuf[32+27] ;
      if (staclu == 0 )  loadroot() ;     //it is root then
      else {
         PartSubDirLevel-- ;
         for (o=0;o<11;o++)  dstr[o] = subnam[o][PartSubDirLevel] ;
         subdirh() ;
      }
   }
}
void GemdDlg::on_setddP_clicked()
{
   fileName = QFileDialog::getExistingDirectory(this, tr("Select dest. dir:"), "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
   if( !fileName.isEmpty() )
   {
      //QByteArray qchar_array = fileName.toLatin1();
      //const char *c_char = qchar_array.data();
      strcpy(DestDir, (char*)fileName.toLatin1().data());
      chdir(DestDir);
      //SetCurrentDirectory(DestDir) ;
   }
}

void GemdDlg::on_filesLB_doubleClicked(const QModelIndex &index)
{
   if (PartSubDirLevel > 0) {
       if (index.row() == 0) { //updir
           DirUp();
       } else {
          opensubd(index.row() - 1);
       }

   } else { opensubd(index.row()); }
}

void GemdDlg::on_opdirP_clicked()
{
     if (PartSubDirLevel > 0) {
        if (ui->filesLB->currentRow() == 0) { //updir
            DirUp();
        } else {
           opensubd(ui->filesLB->currentRow() - 1);
        }
    } else { opensubd(ui->filesLB->currentRow()); }
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
    #endif

    #ifdef WINDOWS
    DosDateTimeToFileTime(fdate, ftime, &fildt);
    SetFileTime(fHandle, &fildt, &fildt, &fildt);
    #else
    if (fHandle) {};
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
    utime(FileName, &fildt) ;
    #endif
}

int GemdDlg::ExtractFile(unsigned char* DirBuffer, int EntryPos)
{
   bool doloop = false;
   int rc = 0;
   unsigned int StartCluster, NextCluster, FileSector;
   unsigned int FileLength, msapos = 0;
   int RemainingBytes;

   #ifdef WINDOWS
   HANDLE flout;
   DWORD  written;
   wchar_t wtext[256];
   #endif
   //load file from drive/image:
   StartCluster = DirBuffer[EntryPos+26]+256*DirBuffer[EntryPos+27] ;
   FileLength = 16777216*DirBuffer[EntryPos+31]+65536*DirBuffer[EntryPos+30]+256*DirBuffer[EntryPos+29]+DirBuffer[EntryPos+28] ;
   RemainingBytes = FileLength;
   fdat = DirBuffer[EntryPos+24]+256*DirBuffer[EntryPos+25] ;
   ftim = DirBuffer[EntryPos+22]+256*DirBuffer[EntryPos+23] ;
   // open save file:
   #ifdef WINDOWS
   mbstowcs(wtext, dstr, strlen(dstr)+1);
   flout = CreateFile(wtext, GENERIC_WRITE + GENERIC_READ, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
   if (flout == INVALID_HANDLE_VALUE) {
   #else
   flout = fopen(dstr,"wb");
   if (flout == NULL) {
   #endif
      QMessageBox::critical(this, dstr, "File open error ", QMessageBox::Cancel, QMessageBox::Cancel);
      ui->errti->setText("Error!");
	  return 1;
   }
   tosave = PartSectorSize*PartSectorsPerCluster ;
   // get sector(s) to load and next cluster
   do {
      FATlookUp(StartCluster, &NextCluster, &FileSector);
      ReadSectors(FileSector, PartSectorsPerCluster, trasb);
      if ( RemainingBytes<(PartSectorSize*PartSectorsPerCluster) ) { tosave = RemainingBytes; }
      #ifdef WINDOWS
      if (WriteFile(flout, trasb, tosave, &written, NULL)) {
        msapos = msapos + written;
      }
      #else
      msapos = msapos+fwrite(trasb, 1, tosave, flout);
      #endif
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
                , flout
                #else
                , dstr
                #endif
                );
   }
   #ifdef WINDOWS
   CloseHandle(flout);
   #else
   fclose(flout);
   #endif
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
   ui->exwht->setText(NameBuffer);
   ui->errti->setText(" ");
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

void SetFATFileDateTime(unsigned char* DirBuffer, int EntryPos, struct stat* FileParams)
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
   memcpy(&tout, trc, sizeof(tm));
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

void GemdDlg::EnterSubDir(unsigned char* DirBuffer, int EntryPos, char* NameBuffer, unsigned int* StartCluster, bool MakeLocalDir, bool EnterLocalDir)
{
   int o;

   pdirpos[PartSubDirLevel] = EntryPos;
   PartSubDirLevel++;
   *StartCluster = DirBuffer[EntryPos+26]+256*DirBuffer[EntryPos+27];
   //Get dir name:
   for(o=0;o<11;o++) {
      NameBuffer[o] = DirBuffer[EntryPos+o] ;
      subnam[o][PartSubDirLevel] = DirBuffer[EntryPos+o];
   }
   NameBuffer[11] = 0 ;
   if(MakeLocalDir){
      #ifdef WINDOWS
      o = mkdir(NameBuffer);
      #else
      o = mkdir(NameBuffer,0777);
      #endif
   }
   if (EnterLocalDir) {
      o = chdir(NameBuffer);
   }
   ui->dirLabel->setText(NameBuffer);
   QCoreApplication::processEvents();
   LoadDirBuf(StartCluster, DirBuffer);
}

bool GemdDlg::EnterUpDir(unsigned char* DirBuffer, char* NameBuffer, unsigned int* StartCluster, unsigned int* UpEntryPos, bool ChangeLocalDir) //returns IsRoot
{
   int o;
   unsigned int EntryPos = 32; //".."
   bool IsRoot = false;

   if (PartSubDirLevel == 0) { //we're already on root level
      return true;
   }
   PartSubDirLevel--;
   *StartCluster = DirBuffer[EntryPos+26]+256*DirBuffer[EntryPos+27];
   //Get dir name:
   for(o=0;o<11;o++) {
      NameBuffer[o] = subnam[o][PartSubDirLevel];
   }
   NameBuffer[11] = 0 ;
   ui->dirLabel->setText(NameBuffer);
   QCoreApplication::processEvents();
   if (PartSubDirLevel == 0) { //we're on root level = definitely topmost and other handling
      ReadSectors(PartFirstRootDirSector, PartRootDirSectors, DirBuffer);
      IsRoot = true;
   } else {
      LoadDirBuf(StartCluster, DirBuffer);
      IsRoot = false;
   }
   if(ChangeLocalDir){
     chdir("..");
   }
   *UpEntryPos = pdirpos[PartSubDirLevel];
   return IsRoot;
}
void GemdDlg::on_DeleteFiles_clicked()
{
   unsigned int CurrentDirPos;
   int  DirLevel, listOffset, FileRemainsLevel;
   char message[256];

   if (ui->filesLB->currentRow() < 0) { return; }
   if ( QMessageBox::warning(this, "Deleting files!",
                             "You are about to delete files and / or directories.\nThey can not be restored!\n\nAre you sure?",
                             QMessageBox::Yes | QMessageBox::Cancel) == QMessageBox::Cancel) { return; }

   //QMessageBox::critical(this, "Deleting!", "Deleting", QMessageBox::Cancel);

   listOffset = (PartSubDirLevel > 0) ? 1 : 0;
   setCursor(QCursor(Qt::WaitCursor));
   DirLevel   = 0;
   FileRemainsLevel = 0;
   pdirpos[0] = 0;
   // Get count of items in listbox
   int litc = ui->filesLB->count()-listOffset;
   for (int p=0;p<litc;p++) { //step through listed files
      // see is item selected:
      if ( ui->filesLB->item(p+listOffset)->isSelected()) { // care for selected only
         //ui->filesLB->item(p+listOffset)->setSelected(false);
         dpos = dirpos[p];// Position in DIR
         if (IsVolume(dirbuf, dpos)) { continue; }    //avoid VOL extraction
         GetFATFileName(dirbuf, dpos, dstr);
         // Recursive subdir extraction Open SubDir, show files, and extract
         // Need to store level, parent & pos in parent
         if (IsSubDir(dirbuf, dpos)) {
            sprintf(message, "Delete subdirectory %s including all files below?", dstr);
            if (QMessageBox::warning(this, dstr, message, QMessageBox::Yes | QMessageBox::Cancel) ==  QMessageBox::Cancel){
               continue; //next entry
            }
            // Store pos in parent, here by selection number
            EnterSubDir(dirbuf, dpos, dstr, &staclu, true, true);
            DirLevel++;
            CurrentDirPos = 64; // 1st 2 entries of sub dir are '.' and '..'
            do{
               bool intoNextSubDir = false;
               for (UINT n=CurrentDirPos; n<PartSectorSize*DirCurrentLenght; n=n+32) {
                  if (IsEmpty(dirbuf, n)) { break; }
                  if (IsDeleted(dirbuf, n)) { continue; }
                  GetFATFileName(dirbuf, n,dstr);
                  if (IsFile(dirbuf, n)) { //no subdir, it's a file
                      sprintf(message, "Delete file %s?", dstr);
                      if (QMessageBox::warning(this, dstr, message, QMessageBox::Yes | QMessageBox::Cancel) ==  QMessageBox::Yes){
                         EraseFile(dirbuf, n);
                      } else { FileRemainsLevel = DirLevel; }
                  }else if (IsSubDir(dirbuf, n)) {
                     // its a subdir -> store pos in dir, leave n loop
                     sprintf(message, "Delete subdirectory %s including all files below??", dstr);
                     if (QMessageBox::warning(this, dstr, message, QMessageBox::Yes | QMessageBox::Cancel) ==  QMessageBox::Yes){
                        EnterSubDir(dirbuf, n, dstr, &staclu, true, true);
                        DirLevel++;
                        intoNextSubDir = true;
                        break;
                     } else { FileRemainsLevel = DirLevel; }
                  }
               }  //n loop end
               if(intoNextSubDir){ intoNextSubDir = false; continue; }
               //we are though
               if (FileRemainsLevel == DirLevel){ //there wss a file left in SubDir
                  WriteCurrentDirBuf(); //rewrite Dir
               }
               //dir up
               EnterUpDir(dirbuf, dstr, &staclu, &CurrentDirPos, true);
               DirLevel--;
               //do not delete dirs not empty
               if (FileRemainsLevel <= DirLevel) {
                  EraseFile(dirbuf, CurrentDirPos); //just mark Clusters as free
               }
               if (FileRemainsLevel > DirLevel) {FileRemainsLevel = DirLevel; }
               if (DirLevel == 0) { //we're on topmost level
                  break;
               }
               CurrentDirPos += 32; // point to next entry
               //are we in subdir? :
               if (dirbuf[0] != 0x2E) { QMessageBox::critical(this, "Curious problem!", "Bounced to bottom of root directory.\n This should never happen!", QMessageBox::Cancel); return; } //If it's not 'dot' entry then we're through with root DIR. If, it's the end of a SUBDIR
            }while( PartSubDirLevel > 0);
            continue; //next main dir entry
         }
         sprintf(message, "Delete File %s?", dstr);
         if (QMessageBox::warning(this, dstr, message, QMessageBox::Yes | QMessageBox::Cancel) ==  QMessageBox::Yes){
            EraseFile(dirbuf, dpos);
         }
      } //
   }  //
   WriteCurrentDirBuf(); //rewrite Dir
   //Write FAT back:
   WriteSectors( PartReservedSectors, PartSectorsPerFAT, PartFATbuffer);
   //Second FAT
   WriteSectors( PartReservedSectors+PartSectorsPerFAT, PartSectorsPerFAT, PartFATbuffer);
   // Refresh Used/free indicator:
   ShowPartitionUsage() ;
   // Need to refresh listbox content
   LoadSubDir((PartSubDirLevel == 0), true) ;

   setCursor(QCursor(Qt::ArrowCursor));
}

void GemdDlg::on_ExtractFiles_clicked()
{
   unsigned int CurrentDirPos;
   int DirLevel, listOffset;

   #ifdef FRANKS_DEBUG
   strcpy(DestDir, "D:\\Projekte\\tools\\build-DrImg\\testoutput");
   chdir(DestDir);
   debug_dirbuf = (unsigned char*)(&dirbuf[64]);
   if (debug_dirbuf) {};
   #endif

   if (ui->filesLB->currentRow() < 0) { return; }
   listOffset = (PartSubDirLevel > 0) ? 1 : 0;
   setCursor(QCursor(Qt::WaitCursor));
   DirLevel   = 0 ;
   pdirpos[0] = 0;
   // Get count of items in listbox
   int litc = ui->filesLB->count()-listOffset;
   for (int p=0;p<litc;p++) { //step through listed files 
      // see is item selected:
      if ( ui->filesLB->item(p+listOffset)->isSelected()) { // care for selected only
         //ui->filesLB->item(p+listOffset)->setSelected(false);
         dpos = dirpos[p];// Position in DIR
         if (IsVolume(dirbuf, dpos)) { continue; }  //avoid VOL extraction
         GetFATFileName(dirbuf, dpos, dstr);
         // Recursive subdir extraction Open SubDir, show files, and extract
         // Need to store level, parent & pos in parent
         if (IsSubDir(dirbuf, dpos)) {
            // Store pos in parent, here by selection number
            EnterSubDir(dirbuf, dpos, dstr, &staclu, true, true);
            DirLevel++;
            CurrentDirPos = 64; // 1st 2 entries of sub dir are '.' and '..'
            do{
               bool intoNextSubDir = false;
               for (UINT n=CurrentDirPos; n<PartSectorSize*DirCurrentLenght; n=n+32) {
                  int rc = LoadEntryName(dirbuf, n, false, false);
                  if (rc == 1) break; //empty entry
                  if (rc == 2) continue; //deleted entry
                  GetFATFileName(dirbuf, n, dstr);
                  if (IsFile(dirbuf, n)) { //no subdir, it's a file
                     ExtractFile(dirbuf, n);
                  }else if (IsSubDir(dirbuf, n)) {
                     // its a subdir -> store pos in dir, leave n loop
                     EnterSubDir(dirbuf, n, dstr, &staclu, true, true);
                     DirLevel++;
                     intoNextSubDir = true;
                     break;
                  }
               }  //n loop end
               if(intoNextSubDir){ intoNextSubDir = false; continue; }
               //we are though, dir up
               EnterUpDir(dirbuf, dstr, &staclu, &CurrentDirPos, true);
               DirLevel--;
               if (DirLevel == 0) { //we're on topmost level
                  break;
               }
               CurrentDirPos += 32; // point to next entry
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

int PadFileName(char *Instr, char* Outstr)
{
   int n, k, c;
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
   for (c=length-1; c>0; c--)
      #ifdef WINDOWS
      if ( SrcFileName[c] == '\\' ) break  ;
      #else
      if ( SrcFileName[c] == '/' ) break  ;
      #endif
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
      if (k>64) k = k&0xDF ; // Make all capital
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
   int m, fx;

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


int MakeSubF(UINT Clun) // Create start cluster of subdirectory
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
   n = ClunOfParent ; //!!!!!!! zero if parent is ROOT
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

bool GemdDlg::AddFileToCurrentDir(char* FilePathName, unsigned char* DirBuffer)
{
   char   FATFileName[256];
   struct stat fparms ;
   unsigned int FileByteLength;
   unsigned int n, m, FileClusterCount, EntryPos, FileSector, CurrentFileCluster, PreviousFileCluster;
   FILE *inFile;

   MakeFATFileName(FilePathName, FATFileName);
   ui->exwht->setText(FATFileName);   // show corrected filename
   QCoreApplication::processEvents();
   //sleep(1); //testing
   // Open file and get it's length:
   // Ignore directories:
   stat( FilePathName , &fparms );
   if (S_IFDIR & fparms.st_mode ) { return(true); } //Frank !!!!!!!!!
   inFile = fopen(FilePathName, "rb");
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
   for (n=0;n<11;n++) DirBuffer[EntryPos+n]=FATFileName[n];
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
      o = 0 ; //flag
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
   // need to get pos first (to write 1 cluster only):
   n = EntryPos/(PartSectorSize*PartSectorsPerCluster);
   if ( PartSubDirLevel == 0 ) WriteSectors( PartFirstRootDirSector+n*PartSectorsPerCluster, PartSectorsPerCluster, DirBuffer);
   else  {
      m = n * PartSectorsPerCluster*PartSectorSize;
      WriteSectors( DirCurrentClustes[n], PartSectorsPerCluster, DirBuffer + m);
   }
   return(true);
}


void GemdDlg::on_AddFiles_clicked()
{
   char FilePathName[256];

   // Multiple file open dialQFileDialogog:
   QStringList SeldFiles = QFileDialog::getOpenFileNames(this, tr("Files to load"), NULL, "ST PRG, TOS, TTP, APP files (*.prg *.tos *.ttp *.app);;Binary files(*.bin);;All files(*.*)");
   if(!SeldFiles.isEmpty())  {

      QCoreApplication::processEvents(); // refresh dialog content
      setCursor(QCursor(Qt::WaitCursor));

      for (QStringList::Iterator iq = SeldFiles.begin(); iq != SeldFiles.end(); ++iq)
      {
         //const char *c_char = qchar_array.data();
         strcpy(FilePathName, (char*)iq->toLatin1().data());   // Filename with path?
         if (!AddFileToCurrentDir(FilePathName, dirbuf)) { break; }
      } // iq for loop end
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

   //  1. Padd entered filename with zeros by need
   //  2. Check is space on partiton
   //  3. Check is name exists
   //  4. Add it to current DIR
   //  5. Create FAT entries - size (1 clust only) ??
   //  6. Clear sectors and add SubDir .. and 2E at start
   //  7. Rewrite medias DIR and FAT sectors!!!
   //  8. Refresh ListBox with DIR content
   //  9. Refresh Used/Free info at bottom
   int m,n;
   unsigned int FileSector, EntryPos;

   QString litet = ui->dirED->text();
   //GetDlgItemText(hDlgWnd, GemCreD , szText , (LPARAM) 64);
   if ( litet.length() == 0 ){
      QMessageBox::critical(this, tr("Invalid filename!"), tr("Enter something"), QMessageBox::Cancel, QMessageBox::Cancel);
      return;
   }
   // Look is name base 8 space:
   c=0;
   for (n=0;n<8;n++) {
      if (litet[n]==' ') c++ ;
   }
   if (c==8) {
      QMessageBox::critical(this, tr("Invalid filename!"), tr("All the spaces..."), QMessageBox::Cancel, QMessageBox::Cancel);
      return;
   }
   strcpy(subdnam,(char*)litet.toLatin1().data());
   res = PadFileName(subdnam, dstr) ;
   if (!res) {
      QMessageBox::critical(this, tr("Invalid filename!"), tr("Must have dot before ext."), QMessageBox::Cancel, QMessageBox::Cancel);
      return;
   }
   // Testing
   ui->exwht->setText(dstr);
   // On free cluster at least needed
   if ( GetFreeClusters() < 1 ) {
     QMessageBox::critical(this, tr("Partition full!"), tr("No place"), QMessageBox::Cancel, QMessageBox::Cancel);
     return;
   }
   res = CheckNameExistsInDir(dstr, dirbuf) ;
   if (!res) {
      QMessageBox::critical(this, tr("Name exists!"), tr("Name already in Dir."), QMessageBox::Cancel, QMessageBox::Cancel);
      return;
   }
   // Now seek first free slot in DIR:
   if (!GetFirstFreeDirSlot(dirbuf, &EntryPos)){
      QMessageBox::critical(this, tr("DIR full!"), tr("Error"), QMessageBox::Cancel, QMessageBox::Cancel);
      return;
   }
   // Copy name at EntryPos, add timestamp and directory flag
   for (n=0;n<11;n++) dirbuf[EntryPos+n]=dstr[n];
   dirbuf[EntryPos+11]=16; // DIR flag
   // Add timestamp:
   time_t tim ;
   struct tm tout;
   #ifdef WINDOWS
   tm * trc;
   #endif

   //tim = time(NULL);
   time(&tim);
   #ifdef WINDOWS
   trc = localtime(&tim);
   memcpy(&tout, trc, sizeof(tm));
   #else
   localtime_r( &tim, &tout);
   #endif
   //SetDlgItemText(hDlgWnd,exwht,dstr); // Testing
   n = (tout.tm_sec/2)|(tout.tm_min<<5)|(tout.tm_hour<<11) ;
   fatTime = n ;
   qhm.setNum(n);
   ui->errti->setText(qhm); //testing
   dirbuf[EntryPos+22] = n;
   dirbuf[EntryPos+23] = n>>8;
   //n = int(FatDate) ;
   n = (tout.tm_mday)|((tout.tm_mon+1)<<5)|( (tout.tm_year-80)<<9) ;
   fatDate = n;
   dirbuf[EntryPos+24] = n;
   dirbuf[EntryPos+25] = n>>8;
   //Set start cluster #
   // Seek first free cluster
   m = SeekFirstFreeCluster(); //we have already checked - there are definitely enough free ones
   n = m;
   dirbuf[EntryPos+26] = n;
   dirbuf[EntryPos+27] = n>>8;
   credirClu[0] = m ; // store spread of created
   credirCnt = 1;
   // seek for next 15 free cluster:
   oldclun = m;
   n = PartSectorSize>>9 ;
   while ( credirCnt<(32/(PartSectorsPerCluster*n)) ) {
      if ((m = SeekNextFreeCluster(m)) == INVALID_VALUE) break ;
      credirClu[credirCnt] = m ;
      // write into FAT
      MarkCluster(oldclun, m);
      credirCnt++ ;
      oldclun = m;
   }
   // mark as last:
   MarkCluster(oldclun, LAST_CLUSTER) ;
   // Write parent DIR with new entry to drive/image:
   if ( PartSubDirLevel == 0 ) { WriteSectors( PartFirstRootDirSector, PartRootDirSectors, dirbuf); }
   else  {
      // need to get pos first:
      n = EntryPos/(PartSectorSize*PartSectorsPerCluster) ;
      m = n * PartSectorsPerCluster*PartSectorSize;
      WriteSectors( DirCurrentClustes[n], PartSectorsPerCluster, dirbuf + m);
   }
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
   //Write FAT back:
   WriteSectors( PartReservedSectors, PartSectorsPerFAT, PartFATbuffer);
   //Second FAT
   WriteSectors( PartReservedSectors+PartSectorsPerFAT, PartSectorsPerFAT, PartFATbuffer);
   // Refresh Used/free indicator:
   ShowPartitionUsage() ;
   // Need to refresh listbox content
   LoadSubDir(false, true) ;
}


void GemdDlg::on_desallP_clicked()
{
   ui->filesLB->clearSelection();
}

void GemdDlg::on_quitP_clicked()
{
   fileclose(drih);
   GemdDlg::close() ;
}
