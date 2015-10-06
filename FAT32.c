//*****************************************************************************
// **** ROUTINES FOR FAT32 IMPLEMATATION OF SD CARD AND RECORDING AND PLAY ****
//*****************************************************************************
//Controller: ATmega644 (Clock: 8 Mhz-internal)
//Compiler: AVR-GCC (Ubuntu cross-compiler avr-g++ -v4.8.2)
//*****************************************************************************



#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FAT32.h"
#include "SD_routines.h"
#include "menucontrol.h"
#include "LCD_driver.h"
#include "midi.h"

//************* external variables *************
volatile unsigned long firstDataSector, rootCluster, totalClusters;
volatile unsigned int  bytesPerSector, sectorPerCluster, reservedSectorCount;
unsigned long unusedSectors, appendFileSector, appendFileLocation, fileSize, appendStartCluster;
unsigned char orginalname[13];
//global flag to keep track of free cluster count updating in FSinfo sector
unsigned char freeClusterCountUpdated;
uint8_t buffer_m[3], save_b[4];
volatile unsigned long time_count;
unsigned char press_f; // press = 1, no press = 0.
unsigned long last_trk_size = 0, tki_p;
unsigned char debug=0;
extern const unsigned char sprintfarg1[] PROGMEM ="the file: %s ?";

const unsigned char backreturn[] PROGMEM ="Press BACK to return ...";
const unsigned char deleting[] PROGMEM ="Deleting..";
const unsigned char filedeleted[] PROGMEM ="File deleted!";
const unsigned char errorclutter[] PROGMEM ="Error in getting cluster";
const unsigned char invalidfilename[] PROGMEM ="Invalid fileName..";
const unsigned char totalmem[] PROGMEM ="Total Memory: ";
const unsigned char freemem[] PROGMEM ="Free Memory: ";
const unsigned char spacebytes[] PROGMEM ="              Bytes";
const unsigned char nofreecluster[] PROGMEM ="No free cluster!";
const unsigned char Save[] PROGMEM =" Do you want to save";
const unsigned char endofchain[] PROGMEM ="End of Cluster Chain";
const unsigned char rusure[] PROGMEM =" Are you sure to delete";
const unsigned char sprintfarg[] PROGMEM =" the file: %s ?";
//***************************************************************************
//Function: to read data from boot sector of SD card, to determine important
//parameters like bytesPerSector, sectorsPerCluster etc.
//Arguments: none
//return: none
//***************************************************************************
unsigned char getBootSectorData (void)
{
  struct BS_Structure *bpb; //mapping the buffer onto the structure
  struct MBRinfo_Structure *mbr;
  struct partitionInfo_Structure *partition;
  unsigned long dataSectors;
  
  unusedSectors = 0;
  
  SD_readSingleBlock(0);
  bpb = (struct BS_Structure *)buffer;
  
  if(bpb->jumpBoot[0]!=0xE9 && bpb->jumpBoot[0]!=0xEB)   //check if it is boot sector
    {
      mbr = (struct MBRinfo_Structure *) buffer;       //if it is not boot sector, it must be MBR
      
      if(mbr->signature != 0xaa55) return 1;       //if it is not even MBR then it's not FAT32
      
      partition = (struct partitionInfo_Structure *)(mbr->partitionData);//first partition
      unusedSectors = partition->firstSector; //the unused sectors, hidden to the FAT
      
      SD_readSingleBlock(partition->firstSector);//read the bpb sector
      bpb = (struct BS_Structure *)buffer;
      if(bpb->jumpBoot[0]!=0xE9 && bpb->jumpBoot[0]!=0xEB) return 1; 
    }

  bytesPerSector = bpb->bytesPerSector;
  sectorPerCluster = bpb->sectorPerCluster;
  reservedSectorCount = bpb->reservedSectorCount;
  rootCluster = bpb->rootCluster;
  firstDataSector = unusedSectors + reservedSectorCount + (bpb->numberofFATs * bpb->FATsize_F32);
  
  dataSectors = bpb->totalSectors_F32
    - bpb->reservedSectorCount
    - ( bpb->numberofFATs * bpb->FATsize_F32);
  totalClusters = dataSectors / sectorPerCluster;
  
  if((getSetFreeCluster (TOTAL_FREE, GET, 0)) > totalClusters)  //check if FSinfo free clusters count is valid
    freeClusterCountUpdated = 0;  //not updated. 
  else
    freeClusterCountUpdated = 1;
  return 0;
}

//***************************************************************************
//Function: to calculate first sector address of any given cluster
//Arguments: cluster number for which first sector is to be found
//return: first sector address
//***************************************************************************
unsigned long getFirstSector(unsigned long clusterNumber)
{
  return (((clusterNumber - 2) * sectorPerCluster) + firstDataSector);
}

//***************************************************************************
//Function: get cluster entry value from FAT to find out the next cluster in the chain
//or set new cluster entry in FAT
//Arguments: 1. current cluster number, 2. get_set (=GET, if next cluster is to be found or = SET,
//if next cluster is to be set 3. next cluster number, if argument#2 = SET, else 0
//return: next cluster number, if if argument#2 = GET, else 0
//****************************************************************************
unsigned long getSetNextCluster (unsigned long clusterNumber,
                                 unsigned char get_set,
                                 unsigned long clusterEntry)
{
  unsigned int FATEntryOffset;
  unsigned long *FATEntryValue;
  unsigned long FATEntrySector;
  unsigned char retry = 0;
  
  //get sector number of the cluster entry in the FAT
  FATEntrySector = unusedSectors + reservedSectorCount + ((clusterNumber * 4) / bytesPerSector) ;
  
  //get the offset address in that sector number
  FATEntryOffset = (unsigned int) ((clusterNumber * 4) % bytesPerSector);
  
  //read the sector into a buffer
  while(retry <10)
    { 
      if(!SD_readSingleBlock(FATEntrySector)) break; retry++;
    } // at this point, we changed the contents of buffer with data from a specific sector in FAT.
  
  //get the cluster address from the buffer
  FATEntryValue = (unsigned long *) &buffer[FATEntryOffset];
  
  if(get_set == GET)
    return ((*FATEntryValue) & 0x0fffffff);
  
  *FATEntryValue = clusterEntry;   //for setting new value in cluster entry in FAT, the value in this postion of buffer was changed and will be written back into FAT of SD card.
  
  SD_writeSingleBlock(FATEntrySector);
  
  return (0);
}

//********************************************************************************************
//Function: to get or set next free cluster or total free clusters in FSinfo sector of SD card
//Arguments: 1.flag:TOTAL_FREE or NEXT_FREE, 
//	     2.flag: GET or SET 
//	     3.new FS entry, when argument2 is SET; or 0, when argument2 is GET
//return: next free cluster, if arg1 is NEXT_FREE & arg2 is GET
//        total number of free clusters, if arg1 is TOTAL_FREE & arg2 is GET
//		  0xffffffff, if any error or if arg2 is SET
//********************************************************************************************
unsigned long getSetFreeCluster(unsigned char totOrNext, unsigned char get_set, unsigned long FSEntry)
{
  SD_readSingleBlock(unusedSectors + 1);  
  
  struct FSInfo_Structure *FS = (struct FSInfo_Structure *) buffer;
  unsigned char error;
    
  if((FS->leadSignature != 0x41615252) || (FS->structureSignature != 0x61417272) || (FS->trailSignature !=0xaa550000))
    return 0xffffffff;
  
  if(get_set == GET) // read data from FSInfo
    {
      if(totOrNext == TOTAL_FREE)
	return(FS->freeClusterCount);
      else // when totOrNext = NEXT_FREE
	return(FS->nextFreeCluster);
    }
  else // update info for FSInfo
    {
      if(totOrNext == TOTAL_FREE)
	FS->freeClusterCount = FSEntry;
      else // when totOrNext = NEXT_FREE
	FS->nextFreeCluster = FSEntry;
      
      error = SD_writeSingleBlock(unusedSectors + 1);	//update FSinfo
    }
  // could use error infor here, if error is 0, then no error in writeSingleBlock. else something wrong in writting.
  return 0xffffffff;
}

//***************************************************************************
//Function: to get DIR/FILE list or a single file address (cluster number) or to delete a specified file
//Arguments: #1 - flag: GET_LIST, GET_FILE or DELETE #2 - pointer to file name (0 if arg#1 is GET_LIST)
//			 #3 - current directory starting Cluster
//return: first cluster of the file, if flag = GET_FILE
//        print file/dir list of the root directory, if flag = GET_LIST
//		  Delete the file mentioned in arg#2, if flag = DELETE
//****************************************************************************
struct dir_Structure* findFiles (unsigned char flag, unsigned char* const fileName, unsigned long startingCluster)
{
  unsigned long cluster, sector, firstSector, firstCluster, nextCluster;
  struct dir_Structure *dir;
  unsigned int i;
  unsigned char j;
  
  cluster = startingCluster;

  while(1) // this is the entry cluster, not data cluster.
    {
      firstSector = getFirstSector (cluster);
      
      for(sector = 0; sector < sectorPerCluster; sector++) // iterate the cluster one sector a time.
	{
	  SD_readSingleBlock (firstSector + sector);
	  	  
	  for(i=0; i<bytesPerSector; i+=32) // iterate the sector one entry a time.
	    {
	      dir = (struct dir_Structure *) &buffer[i];
	      
	      if(dir->name[0] == EMPTY) //indicates end of the file list of the directory
		{
		  return 0;   
		}
	      if((dir->name[0] != DELETED) && (dir->attrib != ATTR_LONG_NAME))
		{
		  if((flag == GET_FILE) || (flag == DELETE))
		    {
		      for(j=0; j<11; j++)
			if(dir->name[j] != fileName[j]) break;
		      if(j == 11)
			{
			  if(flag == GET_FILE) // GET_FILE
			    {
			      appendFileSector = firstSector + sector;
			      appendFileLocation = i;
			      appendStartCluster = (((unsigned long) dir->firstClusterHI) << 16) | dir->firstClusterLO;
			      fileSize = dir->fileSize;
			      return (dir);
			    }	
			  else    //when flag = DELETE
			    {
			      LCD_command(CLEARSCR);
			      LCD_command(FIRST_ROW_START);
			      printf_strPGM((PGM_P)(deleting));
			      firstCluster = (((unsigned long) dir->firstClusterHI) << 16) | dir->firstClusterLO;
			      dir->name[0] = DELETED;    
			      SD_writeSingleBlock (firstSector+sector); //update the corresponding entry.
			      
			      // update total free cluster numbers in FSInfo .
			      freeMemoryUpdate (ADD, dir->fileSize);
			      
			      //update next free cluster entry in FSinfo sector
			      cluster = getSetFreeCluster (NEXT_FREE, GET, 0); 
			      if(firstCluster < cluster)
				getSetFreeCluster (NEXT_FREE, SET, firstCluster);
			      
			      //mark all the clusters allocated to the file as 'free'
			      while(1)  
				{
				  nextCluster = getSetNextCluster (firstCluster, GET, 0);
				  getSetNextCluster (firstCluster, SET, 0);
				  if(nextCluster > 0x0ffffff6) 
				    {/*display_back((PGM_P)(filedeleted),(PGM_P)(backreturn));*/return 0;}
				  firstCluster = nextCluster;
				} 
			    }
			}
		    }
		  
		}
	    }
	}
      
      cluster = (getSetNextCluster (cluster, GET, 0));
      
      if(cluster > 0x0ffffff6)
	return 0; 
      if(cluster == 0) 
	{
	  display_back((PGM_P)(errorclutter),(PGM_P)(backreturn));
	  return 0;
	}
    }
  return 0;
}


//***************************************************************************
//Function: Similar function as findFiles, but this function is for 
//			non-dynamic version of directory entries display: only first step
//			to get how many entries of current directory for -> arrow cursor
//			control.
//Arguments: startingCluster--the starting cluster of the current directory
//return: The number of enties of the current directory
//***************************************************************************
unsigned int numberFiles(unsigned long startingCluster)
{
  unsigned long cluster, sector, firstSector, firstCluster, nextCluster;
  struct dir_Structure *dir;
  unsigned int i,index=0;
  unsigned char j,first_time=1;
  
  cluster = startingCluster; //root cluster
  
  while(1)
    {
      firstSector = getFirstSector (cluster);
      
      for(sector = 0; sector < sectorPerCluster; sector++)
	{
	  SD_readSingleBlock (firstSector + sector);
	  
	  for(i=0; i<bytesPerSector; i+=32)
	    {
	      dir = (struct dir_Structure *) &buffer[i];
	      
	      if(dir->name[0] == EMPTY) //indicates end of the file list of the directory
		{
		  return index;   
		}
	      if((dir->name[0] != DELETED) && (dir->attrib != ATTR_LONG_NAME))
		{
		  index++;
		}
	    }
	}
      
      cluster = (getSetNextCluster (cluster, GET, 0));
      
      if(cluster > 0x0ffffff6)
	return index;
      if(cluster == 0) 
	{
	  display_back((PGM_P)(errorclutter),(PGM_P)(backreturn));
	  return 0;
	}
    }//end of while(1)
  return 0;
}


//***************************************************************************
//Function: Similar function as findFiles, but this function is for 
//			non-dynamic version of directory entries display: the third step
//			to print current and next directory entries and key control function
//Arguments: startingCluster--the starting cluster of the current directory
//			 flag will be pass to the fourth step executeFile function
//			--DELETE for delete file, PLAY for playing file
//***************************************************************************
void listFiles (unsigned char flag, unsigned long startingCluster)
{
  //initialize the menu
  unsigned int current=0;
  uint8_t row=1;
  unsigned char line[15];
  unsigned number=numberFiles(startingCluster);
  locatefile(current,line,startingCluster);
  LCD_command(CLEARSCR);
  LCD_command(FIRST_ROW_START);
  printf_strPGM((PGM_P)(arrow));
  printf_strDM(line);
  if(current+1<=number-1)
    {
      locatefile(current+1,line,startingCluster);
      LCD_command(SECOND_ROW_START);
      printf_strPGM((PGM_P)(space));
      printf_strDM(line);
    }
  
  //wait for input to change the menu
  DDRA=0x00; //set PORTA as input
  while(1)
    {
      switch(PINA)
	{
	case 0x7f://~0x80 sw7 is pushed down, go down
	  {
	    while(PINA!=0xff);//wait the button is released
	    if(current!=number-1)//the current selection is not at the bottom of the menu
	      {
		if(row==1)//arrow in the first row then change it to point second row
		  {
		    row=2;
		    locatefile(current,line,startingCluster);
		    LCD_command(CLEARSCR);
		    printf_strPGM((PGM_P)(space));
		    printf_strDM(line);
		    LCD_command(SECOND_ROW_START);
		    locatefile(current+1,line,startingCluster);
		    printf_strPGM((PGM_P)(arrow));
		    printf_strDM(line);
		  }
		else //row=2->arrow in the second row then scroll the menu down on the screen
		  {
		    row=1;
		    locatefile(current+1,line,startingCluster);
		    LCD_command(CLEARSCR);
		    printf_strPGM((PGM_P)(arrow));
		    printf_strDM(line);
		    if(current+2<=number-1)
		      {
			locatefile(current+2,line,startingCluster);
			LCD_command(SECOND_ROW_START);
			printf_strPGM((PGM_P)(space));
			printf_strDM(line);
		      }
		  }
		current++;
	      }
	    break;
	  }
	  
	case 0xbf://~0x40 sw6 is push down, go up
	  {
	    while(PINA!=0xff);//wait the button is released
	    if(current!=0)
	      {
		if(row=2)//arrow in the second row then change it to point the first row
		  {
		    row=1;
		    locatefile(current-1,line,startingCluster);
		    LCD_command(CLEARSCR);
		    printf_strPGM((PGM_P)(arrow));
		    printf_strDM(line);
		    locatefile(current,line,startingCluster);
		    LCD_command(SECOND_ROW_START);
		    printf_strPGM((PGM_P)(space));
		    printf_strDM(line);			
		  }
		else //row=1->arrow in the first row then scroll the menu up on the screen
		  {
		    row=2;
		    locatefile(current-1,line,startingCluster);
		    LCD_command(CLEARSCR);
		    LCD_command(SECOND_ROW_START);
		    printf_strPGM((PGM_P)(arrow));
		    printf_strDM(line);
		    if(current-2>=0)
		      {
			locatefile(current-2,line,startingCluster);
			LCD_command(FIRST_ROW_START);
			printf_strPGM((PGM_P)(space));
			printf_strDM(line);
		      }
		  }
		current--;	
	      }
	    break;
	  }
	  
	case 0xdf://~0x20 sw5 is pushed down-->confirm selection
	  {
	    while(PINA!=0xff);//wait the button is released
	    unsigned char type=executefile(flag,current,startingCluster);
	    if(type!=4)//if type==4, keep the screen and cursor
	      {
		if(type==3)
		  {
		    current=0;
		    row=1;
		  }
		else if(type==1)//deleting successfully
		  {
		    --current;
		    --number;
		  }
		else ;//type==0,2 keep the current cursor





		
		locatefile(current,line,startingCluster);
		LCD_command(CLEARSCR);
		LCD_command(FIRST_ROW_START);
		printf_strPGM((PGM_P)(arrow));
		printf_strDM(line);
		if(current+1<=number-1)
		  {
		    locatefile(current+1,line,startingCluster);
		    LCD_command(SECOND_ROW_START);
		    printf_strPGM((PGM_P)(space));
		    printf_strDM(line);
		  }
	      }
	    break;
	  }
	  
	  
	case 0xef://~0x10 sw4 is pushed down-->go back
	  {
	    while(PINA!=0xff);//wait the button is release
	    return;
	    break;
	  }
	default:
	  break;
	  
	}//end of switch
    }//end of while(1)
}

//***************************************************************************
//Function: Similar function as findFiles, but this function is for 
//			non-dynamic version of directory entries display: the second step
//			to copy the characters from directory entry to the string to be 
//			displayed (Assemble directory entry name strings
//Arguments: startingCluster--the starting cluster of the current directory
//			 flag will be pass to the third step listFile function
//			--DELETE for delete file, PLAY for playing file
//			fileindex is the current directory entry that cursor point to
//			line is the destination string that entry name string will 
//			be copied to.
//***************************************************************************
void locatefile(unsigned int fileindex, unsigned char* line, unsigned long startingCluster)
{
  unsigned long cluster, sector, firstSector, firstCluster, nextCluster;
  struct dir_Structure *dir;
  unsigned int i,index=0;
  unsigned char j;
  
  cluster = startingCluster; 
  
  while(1)
    {
      firstSector = getFirstSector (cluster);
      
      for(sector = 0; sector < sectorPerCluster; sector++)
	{
	  SD_readSingleBlock (firstSector + sector);
	  
	  for(i=0; i<bytesPerSector; i+=32)
	    {
	      dir = (struct dir_Structure *) &buffer[i];
	      if(dir->name[0] == EMPTY) //indicates end of the file list of the directory
		return;   
	      if((dir->name[0] != DELETED) && (dir->attrib != ATTR_LONG_NAME))
		{
		  if(index==fileindex)
		    {
		      if((dir->attrib != 0x10) && (dir->attrib != 0x08))
			{
			  unsigned char removespace=0;
			  for(j=0; j<8; j++)
			    {
			      if(dir->name[j]!=' ')
				line[removespace++]=dir->name[j];
			    }
			  line[removespace]='.';
			  unsigned char dotposition=removespace;
			  removespace++;
			  
			  for(j=8;j<11;j++)
			    {
			      if(dir->name[j]!=' ')
				line[removespace++]=dir->name[j];
			    }
			  if(dir->name[8]==' ' && dir->name[9]==' ' && dir->name[10]==' ')//there is no extension name
			    line[dotposition]='\0';
			  else
			    line[removespace]='\0';
			  return;					
			}
		      else
			{
			  if(dir->attrib == 0x08)//root directory
			    {
			      line[0]='<';
			      line[1]='/';
			      line[2]='>';
			      line[3]='\0';
			    }
			  else//0x10  non root directory
			    {
			      int removespace=1;
			      line[0]='<';
			      for(j=0; j<8; j++)
				{
				  if(dir->name[j]!=' ')
				    line[removespace++]=dir->name[j];
				}
			      
			      line[removespace]='.';
			      unsigned char dotposition=removespace;
			      removespace++;
			      
			      for(j=8;j<11;j++)
				{
				  if(dir->name[j]!=' ')
				    line[removespace++]=dir->name[j];
				}
			      
			      if(dir->name[8]==' ' && dir->name[9]==' ' && dir->name[10]==' ')//there is no extension name
				{
				  line[dotposition++]='>';
				  line[dotposition]='\0';
				}
			      else
				{
				  line[removespace++]='>';
				  line[removespace]='\0';
				}
			    }
			  return;
			}
		    }//fileindex==index
		  index++;
		}//if not deleted or long file name
	    }//end of for 32 entry
	}//end of for 8 sector
      
      cluster = (getSetNextCluster (cluster, GET, 0));
      
      if(cluster > 0x0ffffff6)
	return;
      if(cluster == 0) 
	{
	  display_back((PGM_P)(errorclutter),(PGM_P)(backreturn));
	  return;
	}
    }//end of while(1)
  return;
}

//***************************************************************************
//Function: Similar function as findFiles, but this function is for 
//			non-dynamic version of directory entries display: the fourth step
//			to execute the selected entry. Depend on flag variable, this function
//			will call different function to execute.
//Arguments: startingCluster--the starting cluster of the current directory
//			 flag will be pass to the third step listFile function
//			--DELETE for delete file, PLAY for playing file
//			fileindex is the current directory entry that cursor point to.
//Output: for after executing cursor and line control:
//		  1,0 for confirm or cancel delete return
//		  2 for clear screen keep current cursor;
//		  3 for reset current cursor to the first;
//		  4 for keep current cursor;
//***************************************************************************
unsigned char executefile(unsigned char flag, unsigned int fileindex, unsigned long startingCluster)
{
  unsigned long cluster, sector, firstSector, firstCluster, nextCluster;
  struct dir_Structure *dir;
  unsigned int i,index=0;
  cluster = startingCluster; 
  unsigned char nodotfilename[15];
  
  while(1)
    {
      firstSector = getFirstSector (cluster);
      
      for(sector = 0; sector < sectorPerCluster; sector++)
	{
	  SD_readSingleBlock (firstSector + sector);
	  
	  for(i=0; i<bytesPerSector; i+=32)
	    {
	      dir = (struct dir_Structure *) &buffer[i];
	      if(dir->name[0] == EMPTY) //indicates end of the file list of the directory
		return 4;   
	      if((dir->name[0] != DELETED) && (dir->attrib != ATTR_LONG_NAME))
		{
		  if(fileindex==index)
		    {
		      if((dir->attrib != 0x10) && (dir->attrib != 0x08))
			{
			  unsigned char namebuffer[15];
			  unsigned char removespace=0;
			  unsigned char j;
			  for(j=0; j<11;j++)//save no dot filename
			    nodotfilename[j]=dir->name[j];
			  
			  nodotfilename[11]='\0';

			  for(j=0; j<8; j++)
			    {
			      if(dir->name[j]!=' ')
				namebuffer[removespace++]=dir->name[j];
			    }
			  namebuffer[removespace]='.';
			  unsigned char dotposition=removespace;
			  removespace++;
			  for(j=8;j<11;j++)
			    {
			      if(dir->name[j]!=' ')
				namebuffer[removespace++]=dir->name[j];
			    }
			  if(dir->name[8]==' ' && dir->name[9]==' ' && dir->name[10]==' ')//there is no extension name
			    {
			      namebuffer[dotposition]='\0';
			    }
			  else
			    {
			      namebuffer[removespace]='\0';					
			    }
			  if(flag==DELETE)
			    {
				  //confirm_delete(file)
				  LCD_command(CLEARSCR);
				  LCD_command(FIRST_ROW_START);
				  printf_strPGM((PGM_P)(rusure)); //" Are you sure to delete"
				  
				  LCD_command(SECOND_ROW_START);
				  char szBuffer[25];
				  char temp[25];
	
				  strcpy_P(temp,(const char*)sprintfarg);

				  sprintf(szBuffer,(const char*)temp,namebuffer);

				  printf_strDM((unsigned char*)szBuffer);	//" the file: xxx ?"
				  
				  DDRA=0x00; //set PORTA as input
				  while(1)
				    {
				      switch(PINA)
					{
					case 0xdf://~0x10 sw5 is pushed down-->confirm selection
					  {
					    while(PINA!=0xff);//wait the button is released
					    //deleteFile(fileName,startingCluster);

					    
					    //findFiles (DELETE, namebuffer,startingCluster);
					    
					    unsigned long cluster, sector, firstSector, firstCluster, nextCluster;
					    struct dir_Structure *dir;
					    unsigned int i;
					    unsigned char j;
					    
					    cluster = startingCluster;
					    while(1) // this is the entry cluster, not data cluster.
					      {
						firstSector = getFirstSector (cluster);
						
						for(sector = 0; sector < sectorPerCluster; sector++) // iterate the cluster one sector a time.
						  {
						    SD_readSingleBlock (firstSector + sector);
						    
						    for(i=0; i<bytesPerSector; i+=32) // iterate the sector one entry a time.
						      {
							dir = (struct dir_Structure *) &buffer[i];
							
							if(dir->name[0] == EMPTY) //indicates end of the file list of the directory
							  {
							    return 0;   
							  }
							if((dir->name[0] != DELETED) && (dir->attrib != ATTR_LONG_NAME))
							  {
							    if(flag == DELETE)
							      {
								for(j=0; j<11; j++)
								  if(dir->name[j] != nodotfilename[j]) break;
								
								if(j == 11)
								  {
								    LCD_command(CLEARSCR);
								    LCD_command(FIRST_ROW_START);
								    printf_strPGM((PGM_P)(deleting));
								    firstCluster = (((unsigned long) dir->firstClusterHI) << 16) | dir->firstClusterLO;
								    dir->name[0] = DELETED;    
								    SD_writeSingleBlock (firstSector+sector); //update the corresponding entry.
								    
								    // update total free cluster numbers in FSInfo .
								    freeMemoryUpdate (ADD, dir->fileSize);
								    
								    //update next free cluster entry in FSinfo sector
								    cluster = getSetFreeCluster (NEXT_FREE, GET, 0); 
								    if(firstCluster < cluster)
								      getSetFreeCluster (NEXT_FREE, SET, firstCluster);
								    
								    //mark all the clusters allocated to the file as 'free'
								    while(1)  
								      {
									nextCluster = getSetNextCluster (firstCluster, GET, 0);
									getSetNextCluster (firstCluster, SET, 0);
									if(nextCluster > 0x0ffffff6) 
									  {display_back((PGM_P)(filedeleted),(PGM_P)(backreturn));return 1;}
									firstCluster = nextCluster;
								      } 
								    
								  }
							      }
							    
							  }
						      }
						  }
						
						cluster = (getSetNextCluster (cluster, GET, 0));
						
						if(cluster > 0x0ffffff6)
						  return 0; 
						if(cluster == 0) 
						  {
						    display_back((PGM_P)(errorclutter),(PGM_P)(backreturn));
						    return 0;
						  }
					      }
					    
					    break;
					  }
					  
					case 0xef://~0x10 sw4 is pushed down-->go back
					  {
					    while(PINA!=0xff);//wait the button is released
					    return 0;
					    break;
					  }
					  
					default:
					  break;
					}
				    }
				  
			    }
			  else if(flag==PLAY)
			    {
			      midiplay(namebuffer,((unsigned long)dir->firstClusterHI)<<16|dir->firstClusterLO);
			      return 0;
			    }
			  else
			    {
			      disply_fileAttrb(namebuffer,dir->fileSize);
			      return 2;//clear screen keep current cursor;
			    }
			}
		      else
			{
			  if(dir->attrib == 0x08)//root directory
			    {
			      return 4;//
			    }
			  else//0x10  non root directory
			    {
			      unsigned char namebuffer[15];
			      int removespace=1;
			      unsigned char j;
			      namebuffer[0]='<';
			      for(j=0; j<8; j++)
				{
				  if(dir->name[j]!=' ')
				    namebuffer[removespace++]=dir->name[j];
				}
			      
			      namebuffer[removespace]='.';
			      unsigned char dotposition=removespace;
			      removespace++;
			      
			      for(j=8;j<11;j++)
				{
				  if(dir->name[j]!=' ')
				    namebuffer[removespace++]=dir->name[j];
				}
			      
			      if(dir->name[8]==' ' && dir->name[9]==' ' && dir->name[10]==' ')//there is no extension name
				{
				  namebuffer[dotposition++]='>';
				  namebuffer[dotposition]='\0';
				}
			      else
				{
				  namebuffer[removespace++]='>';
				  namebuffer[removespace]='\0';
				}
			      
			      if(strcmp((char*)namebuffer,"<.>")==0||strcmp((char*)namebuffer,"<..>")==0)//if it is . or .. directory, disable it traverse going deeper: we use "BACK" key to go back
				{
				  return 4;//keep current cursor
				}
			      else
				{
				  unsigned long nextDirectory=((unsigned long)dir->firstClusterHI)<<16|dir->firstClusterLO;
				  listFiles(flag,nextDirectory);
				  return 3;//reset current cursor to the first
				}
			      
			    }
			}
		    }//fileindex==index
		  index++;
		}//if not deleted or long file name
	    }//end of for 32 entry
	}//end of for 8 sector
      
      cluster = (getSetNextCluster (cluster, GET, 0));
      
      if(cluster > 0x0ffffff6)
	return 4;
      if(cluster == 0) 
	{
	  display_back((PGM_P)(errorclutter),(PGM_P)(backreturn));
	  return 2;
	}
    }//end of while(1)
  return 4;
}


//***************************************************************************
//Function: to convert normal short file name into FAT format
//Arguments: pointer to the file name
//return: 0, if successful else 1.
//***************************************************************************
unsigned char convertFileName (unsigned char* const fileName)
{
  unsigned char fileNameFAT[11];
  unsigned char j, k;
  unsigned char flag=0;
  
  for(j=0; j<12; j++)
    if(fileName[j] == '.') break;
  
  if(j==12)//there is no '.' -->no extension name for the file
    {
      j=strlen((char*)fileName);
      flag=1;
    }
  
  if(j>8) {
    display_back((PGM_P)(invalidfilename),(PGM_P)(backreturn)); 
    return 1;}
    
  for(k=0; k<j; k++) //setting file name
    fileNameFAT[k] = fileName[k];
  
  for(k=j; k<=7; k++) //filling file name trail with blanks
    fileNameFAT[k] = ' ';
  
  if(!flag)
    j++;
  
  for(k=8; k<11; k++) //setting file extention
    {
      if(fileName[j] != 0)
	fileNameFAT[k] = fileName[j++];
      else //filling extension trail with blanks
	while(k<11)
	  fileNameFAT[k++] = ' ';
    }
  
  for(j=0; j<11; j++) //converting small letters to caps
    if((fileNameFAT[j] >= 0x61) && (fileNameFAT[j] <= 0x7a))
      fileNameFAT[j] -= 0x20;
  
  for(j=0; j<11; j++)
    fileName[j] = fileNameFAT[j];

  fileName[11]='\0';

  return 0;
}

//***************************************************************************
//Function: to generate the different MIDI recording file.
//Arguments: pointer to the file name.
//***************************************************************************
void createName(unsigned char* filename)
{
  int n = 1;
  char str[3];
  strcpy((char*)orginalname,(char*)filename);
  convertFileName(filename);  
 
  while(readFile(VERIFY, filename)) // readFile(VERIFY, fileName) return 1 if thie file is already in root directory.
    {
      n++;
      sprintf(str, "%d", n);
      
      if(strlen(str) == 1) filename[7] = str[0];
      else if(strlen(str) == 2)
	{
	  filename[6] = str[0];
	  filename[7] = str[1];
	}
    
    }
  
  orginalname[6]=filename[6];
  orginalname[7]=filename[7];
}


//***************************************************************************
// Function for FAT writing files of controlling updating buffer to sector 
//			and secotr to cluster
//***************************************************************************
void updateFAT(unsigned int* a, unsigned char* b, unsigned long* pre_clu, unsigned long* curr_clu, volatile unsigned long* start_block)
{
  last_trk_size++;
  if(*a == bytesPerSector)
    {
      SD_writeSingleBlock (*start_block + *b);
      *a = 0;
      (*b)++;
      if(*b == sectorPerCluster)
	{
	  *pre_clu = *curr_clu;
	  *curr_clu = searchNextFreeCluster(*pre_clu);
	  
	  if(*curr_clu == 0)
	    {
	      //print "No free cluster!" on LCD
	      display_back((PGM_P)(nofreecluster),(PGM_P)(backreturn));
	      return;
	    }
	    *start_block = getFirstSector (*curr_clu);
	    getSetNextCluster(*pre_clu, SET, *curr_clu); //connect prevCluster to cluster.
	    getSetNextCluster(*curr_clu, SET, EOF);   //last cluster of the file, marked EOF
	    *b = 0;
	}
    }
}

//***************************************************************************************************
//Function: to create a file in FAT32 format in the root directory if given file name does not exist; 
//Arguments: pointer to the file name
//return: none
//***************************************************************************************************
//
//
//Before we use this function, we need to make sure the filename is not exisiting in root directory.
//and the filename is compatible. We can use convertfilename function to convert correct name to 
//FAT format.We can use readFile with flag=VERIFY to make sure we have a different name.
void writeFile (unsigned char *fileName)
{
  unsigned char j, data, error, fileCreatedFlag = 0, start = 0, sector=0;
  unsigned int i, firstClusterHigh=0, firstClusterLow=0;  //value 0 is assigned just to avoid warning in compilation
  struct dir_Structure *dir;
  unsigned long cluster, nextCluster, prevCluster, firstSector, clusterCount, extraMemory;
  
  //we should print something like "Recording midi12.mid now..." here on the LCD.
  char szBuffer[25];
  sprintf(szBuffer,"Recording %s..",orginalname);
  LCD_command(CLEARSCR);
  LCD_command(FIRST_ROW_START);
  printf_strDM((unsigned char*)szBuffer);

  cluster = getSetFreeCluster (NEXT_FREE, GET, 0); //find the total number of free clusters in SD card.
  if(cluster > totalClusters)
    cluster = 2;
  
  cluster = searchNextFreeCluster(cluster); // find next free cluster after a specified cluster.
  
  if(cluster == 0)
    {
      //print "No free cluster!" on LCD
      display_back((PGM_P)(nofreecluster),(PGM_P)(backreturn));
      return;
   }

  // cluster is the starting data cluster for this file. 
  getSetNextCluster(cluster, SET, EOF);   //last cluster of the file, marked EOF, it is an empty file. 
  
  //initial attribute for this file.
  firstClusterHigh = (unsigned int) ((cluster & 0xffff0000) >> 16 );
  firstClusterLow = (unsigned int) ( cluster & 0x0000ffff);
  fileSize = 0;
  
  startBlock = getFirstSector (cluster);
  unsigned long firstblock = startBlock;
  i=98; //when i = 511, buffer(sector) is full.
  j=0; //when j = sectorPerCluster (7) , a cluster is full. We need to find another free cluster. After each round, update size in byte.

  initialize();
  
  buffer_m[2]=0x00;
  DDRA=0x00;
  unsigned char dt[4];
  unsigned char n_dt;
  int g;
  
  while(1)
    {
      TIM16_WriteOCR1A(CLK/(PSCALER*2000)-1);
      press_f=0;
      sei();
      while(buffer_m[2]==0x00)
	{
	  while ( !(UCSR0A & (1<<RXC0)) )
	    {
	      if(PINA==0xdf)//confirm button is pressed
		{
		  while(PINA!=0xff);//wait the button is release
		  goto go_to_label;
		}
	    }
	  
	  cli();
	  n_dt=DeltaTimeConversion(time_count,0, dt);
	  time_count = 0;
	  
	  for(g = 0; g<n_dt; g++)
	    {
	      buffer[i++] = dt[g];
	      updateFAT(&i, &j, &prevCluster, &cluster, &startBlock);
	    }
	  buffer_m[0]=UDR0;
	  buffer[i++] = buffer_m[0];
	  updateFAT(&i, &j, &prevCluster, &cluster, &startBlock);
	  
	  while ( !(UCSR0A & (1<<RXC0)) ) ;
	  buffer_m[1]=UDR0;
	  buffer[i++] = buffer_m[1];
	  updateFAT(&i, &j, &prevCluster, &cluster, &startBlock);
	  
	  while ( !(UCSR0A & (1<<RXC0)) ) ;
	  buffer_m[2]=UDR0;
	  buffer[i++] = buffer_m[2];
	  updateFAT(&i, &j, &prevCluster, &cluster, &startBlock);
	}
		
      press_f=1;
      
      while(buffer_m[2]!=0x00)
	{
	  TIM16_WriteOCR1A((unsigned int)roundfloat(ocr[buffer_m[1]-21]));
	  sei();			
	  while ( !(UCSR0A & (1<<RXC0)) )
	    {
	      ;
	    }

	  cli();
	  n_dt=DeltaTimeConversion(time_count,1,dt);
	  time_count = 0;
	  
	  for(g = 0; g<n_dt; g++)
	    {
	      buffer[i++] = dt[g];
	      updateFAT(&i, &j, &prevCluster, &cluster, &startBlock);
	    }
	  
	  buffer_m[0]=UDR0;
	  buffer[i++] = buffer_m[0];
	  updateFAT(&i, &j, &prevCluster, &cluster, &startBlock);
	  
	  while ( !(UCSR0A & (1<<RXC0)) );
	  buffer_m[1]=UDR0;
	  buffer[i++] = buffer_m[1];
	  updateFAT(&i, &j, &prevCluster, &cluster, &startBlock);

	  while ( !(UCSR0A & (1<<RXC0)) );
	  buffer_m[2]=UDR0;
	  buffer[i++] = buffer_m[2];
	  updateFAT(&i, &j, &prevCluster, &cluster, &startBlock);
	}

    }
  
 go_to_label:
  
  buffer[i++] = 0x00;
  updateFAT(&i, &j, &prevCluster, &cluster, &startBlock);
  buffer[i++] = 0xFF;
  updateFAT(&i, &j, &prevCluster, &cluster, &startBlock);
  buffer[i++] = 0x2F;
  updateFAT(&i, &j, &prevCluster, &cluster, &startBlock);
  buffer[i++] = 0x00;
  updateFAT(&i, &j, &prevCluster, &cluster, &startBlock);
  
  SD_writeSingleBlock(startBlock + j);
  getSetNextCluster(prevCluster, SET, cluster); //connect prevCluster to cluster.
  getSetNextCluster(cluster, SET, EOF);

  SD_readSingleBlock(firstblock);
  buffer[86]=((last_trk_size+8)&0xff000000) >> 24;
  buffer[87]=((last_trk_size+8)&0x00ff0000) >> 16;
  buffer[88]=((last_trk_size+8)&0x0000ff00) >> 8;
  buffer[89]=(last_trk_size+8)&0x000000ff;

  SD_writeSingleBlock(firstblock);
  
  fileSize = last_trk_size + 98;
  
  getSetFreeCluster(NEXT_FREE, SET, cluster); //update FSinfo next free cluster entry
  
  prevCluster = rootCluster; //root cluster
  
  while(1)
    {
      firstSector = getFirstSector (prevCluster);
      
      for(sector = 0; sector < sectorPerCluster; sector++)
	{
	  SD_readSingleBlock (firstSector + sector);
	  
	  
	  for(i=0; i<bytesPerSector; i+=32)
	    {
	      dir = (struct dir_Structure *) &buffer[i];
	      
	      if((dir->name[0] == EMPTY) || (dir->name[0] == DELETED))  //looking for an empty slot to enter file info
		{
		  for(j=0; j<11; j++)
		    dir->name[j] = fileName[j]; // enter the filename
		  
		  dir->attrib = ATTR_ARCHIVE;	//settting file attribute as 'archive'
		  dir->NTreserved = 0;			//always set to 0
		  dir->timeTenth = 0;			//always set to 0
		  dir->createTime = 0; 	//setting time of file creation, obtained from RTC
		  dir->createDate = 0; 	//setting date of file creation, obtained from RTC
		  dir->lastAccessDate = 0;   	//date of last access ignored
		  dir->writeTime = 0;  	//setting new time of last write, obtained from RTC
		  dir->writeDate = 0;  	//setting new date of last write, obtained from RTC
		  dir->firstClusterHI = firstClusterHigh;
		  dir->firstClusterLO = firstClusterLow;
		  dir->fileSize = fileSize;
		  
		  SD_writeSingleBlock (firstSector + sector);
		      
		  freeMemoryUpdate (REMOVE, fileSize); //updating free memory count in FSinfo sector
	          goto savefile;
		}
	    }
	}
      
      cluster = getSetNextCluster (prevCluster, GET, 0);
      
      if(cluster > 0x0ffffff6)
	{
	  if(cluster == EOF)   //this situation will come when total files in root is multiple of (32*sectorPerCluster)
	    {  
	      cluster = searchNextFreeCluster(prevCluster); //find next cluster for root directory entries
	      getSetNextCluster(prevCluster, SET, cluster); //link the new cluster of root to the previous cluster
	      getSetNextCluster(cluster, SET, EOF);  //set the new cluster as end of the root directory
	    } 
	  
	  else
	    {	
	      display_back((PGM_P)(errorclutter),(PGM_P)(backreturn)); 
	      return;
	    }
	}
      if(cluster == 0) {display_back((PGM_P)(errorclutter),(PGM_P)(backreturn)); return;}
      
      prevCluster = cluster;
    }
 savefile:

  // save file?
  LCD_command(CLEARSCR);
  LCD_command(FIRST_ROW_START);
  printf_strPGM((PGM_P)(Save)); //" Do you want to save"
  
  LCD_command(SECOND_ROW_START);
  char temp[25];
  
  strcpy_P(temp,(const char*)sprintfarg1);
  sprintf(szBuffer,(const char*)temp,orginalname);
  printf_strDM((unsigned char*)szBuffer);	//" the file: xxx ?"
  
  while(1)
    {
      switch(PINA)
	{
	case 0xef://~0x10 sw4 is pushed down-->delete the file
	  {
	    while(PINA!=0xff);//wait the button is released
	    //deleteFile(fileName,2);
	    findFiles (DELETE, fileName,rootCluster);
	    return;
	  }
	  
	case 0xdf://~0x10 sw5 is pushed down-->confirm selection
	  {
	    while(PINA!=0xff);//wait the button is released
	    
	    return;
	    break;
	  }
	default:
	  break;
	}
    }
  return;
}


//***************************************************************************
// Function for FAT reading files of controlling updating buffer to sector 
//			and secotr to cluster
//***************************************************************************
void readupdate(unsigned* a, unsigned char* b, unsigned long n, unsigned long* start_block, unsigned long* curr_clust)
{
  unsigned long c;
  for(c = 0; c< n; c++)
    {
      if(n==4) save_b[c] = buffer[*a];
      (*a)++;
      tki_p++;
      if((*a) == bytesPerSector)
	{
	  *a = 0;
	  (*b)++;
	  if( *b == sectorPerCluster)
	    {
	      *curr_clust = getSetNextCluster(*curr_clust, GET, 0);
	      *start_block = getFirstSector (*curr_clust);
	      *b = 0;
	    }
	  SD_readSingleBlock(*start_block + *b);
	}
    }
}

//***************************************************************************************************
//Function: to read a file in FAT32 format in the root directory and play it with speaking at same time 
//Arguments: filename: pointer to the file name
//			 startingCluster the starting cluster of the current directory
//return: none
//***************************************************************************************************
void midiplay(unsigned char* filename, unsigned long startingCluster)
{
  char szBuffer[25];
  unsigned long firstSector, tempo, trk_size, deltatime;
  unsigned i,  tick_per_qnote, ntrks, nt = 0;
  unsigned char j = 0, n;
  char m;
  float realtime;
  firstSector = getFirstSector (startingCluster);
  SD_readSingleBlock(firstSector);
  
  init_sound();

  //get number of tracks
  ntrks=((buffer[10]<<8)+buffer[11]);
  
  //get quarter-note
  tick_per_qnote=((buffer[12]<<8)+buffer[13]);

  //get real time of a quarter-note
  for(i = 0; i < bytesPerSector; i++)
    {
      if(buffer[i] == 0xFF && buffer[i+1] == 0x51)
	{
	  tempo=((unsigned long)buffer[i+3]<<16)+((unsigned long)buffer[i+4]<<8)+buffer[i+5];
	  break;
	}
    }
  
  i = 18;   
  trk_size = ((unsigned long)buffer[i]<<24)+((unsigned long)buffer[i+1]<<16)+((unsigned long)buffer[i+2]<<8)+buffer[i+3];
  i = 22;
  unsigned char note, temp_command,velocity;
  
  for(; nt < ntrks; nt++)
    {
      LCD_command(CLEARSCR);
      LCD_command(FIRST_ROW_START);
      printf_strDM((unsigned char*)"Now playing ");
      printf_strDM((unsigned char*)filename);
      printf_strDM((unsigned char*)"..");
      LCD_command(SECOND_ROW_START);
      sprintf(szBuffer,"Track %d",nt+1);
      printf_strDM((unsigned char*)szBuffer);
      
      tki_p=0;
      n = 0;
      while(buffer[i] & 0x80) // get delta time bytes
	{
	  readupdate(&i, &j, 1, &firstSector, &startingCluster);
	}
      
      readupdate(&i, &j, 1, &firstSector, &startingCluster);
      
      while(tki_p < trk_size)
  	{
	  if(buffer[i]&0x80) temp_command = buffer[i];  // for running mode
	  
	  if(buffer[i] == 0xff) //meta-event
	    {
	      if(buffer[i+1]==0x2f)
		readupdate(&i, &j, 3, &firstSector, &startingCluster);
	      else
		{
		  readupdate(&i, &j, 2, &firstSector, &startingCluster);
		  readupdate(&i, &j, buffer[i]+1, &firstSector, &startingCluster);
		  n = 0;
		  while(buffer[i] & 0x80) // get delta time bytes
		    {
		      readupdate(&i, &j, 1, &firstSector, &startingCluster);
		    }
		  
		  readupdate(&i, &j, 1, &firstSector, &startingCluster);
		}
	    }
	  else
	    {
	      if(buffer[i] < 0xf0)  //midi voice event
       		{
		  switch (buffer[i] & 0xf0)
		    {
		    case 0x80 :
		      readupdate(&i, &j, 1, &firstSector, &startingCluster);
		      note = buffer[i];
		      readupdate(&i, &j, 2, &firstSector, &startingCluster);
		      n = 0;
		      while(buffer[i] & 0x80) // get delta time bytes
			{
			  save_b[n++] = buffer[i]; 
			  readupdate(&i, &j, 1, &firstSector, &startingCluster);
			}
		      save_b[n] = buffer[i];
		      readupdate(&i, &j, 1, &firstSector, &startingCluster);
		      deltatime = 0;
		      for(m = n; m >=0; m--) // calculate delta time
			{
			  deltatime += (save_b[m]&0x7f) << 7*(n-m) ;
			}
		      
		      realtime=deltatime/(float)tick_per_qnote*(tempo/1000000.0);
		      realplay(realtime,note,0);
		      break;
		      
		    case 0xa0 :				
		    case 0xb0 :			
		    case 0xe0 :	
		      readupdate(&i, &j, 3, &firstSector, &startingCluster);
		      n = 0;
		      while(buffer[i] & 0x80) // get delta time bytes
			{
			  readupdate(&i, &j, 1, &firstSector, &startingCluster);
			}
		      readupdate(&i, &j, 1, &firstSector, &startingCluster);
		      break;
		      	      
		    case 0x90 :
		      {
			readupdate(&i, &j, 1, &firstSector, &startingCluster);
			note = buffer[i];
			readupdate(&i, &j, 1, &firstSector, &startingCluster);
			velocity = buffer[i];
			readupdate(&i, &j, 1, &firstSector, &startingCluster);
			n = 0;
			while(buffer[i] & 0x80) // get delta time bytes
			  {
			    save_b[n++] = buffer[i]; 
			    readupdate(&i, &j, 1, &firstSector, &startingCluster);
			  }
			save_b[n] = buffer[i];
			readupdate(&i, &j, 1, &firstSector, &startingCluster);
			deltatime = 0;
			for(m = n; m >=0; m--) // calculate delta time
			  {
			    deltatime += (save_b[m]&0x7f) << 7*(n-m) ;
			  }
			realtime=deltatime/(float)tick_per_qnote*(tempo/1000000.0);
			
			unsigned char temp=realplay(realtime,note,velocity);
			if(temp==1)
			  goto go_to_label;
			else if(temp==2)
			  {
			    readupdate(&i, &j, trk_size-tki_p, &firstSector, &startingCluster);//fast forwart to the next track
			    goto next_track;
			  }
			break;
		      }
		    case 0xc0:
		    case 0xd0:
		      readupdate(&i, &j, 2, &firstSector, &startingCluster);
		      n = 0;
		      while(buffer[i] & 0x80) // get delta time bytes
			{
			  readupdate(&i, &j, 1, &firstSector, &startingCluster);
			}
		      readupdate(&i, &j, 1, &firstSector, &startingCluster);
		      break;
		      
		    default:	
		      if(temp_command == 0xff) 
			{
			  if(buffer[i]==0x2f)
			    readupdate(&i, &j, 2, &firstSector, &startingCluster);
			  else
			    {
			      readupdate(&i, &j, 1, &firstSector, &startingCluster);
			      readupdate(&i, &j, buffer[i]+1, &firstSector, &startingCluster);
			      n = 0;
			      while(buffer[i] & 0x80) // get delta time bytes
				{
				  readupdate(&i, &j, 1, &firstSector, &startingCluster);
				}
			      
			      readupdate(&i, &j, 1, &firstSector, &startingCluster);
			    }
			}
		      else
			{
			  if(temp_command < 0xf0)
			    {
			      switch(temp_command & 0xf0)
				{
				case 0x80 :
				  note = buffer[i];
				  readupdate(&i, &j, 2, &firstSector, &startingCluster);
				  n = 0;
				  while(buffer[i] & 0x80) // get delta time bytes
				    {
				      save_b[n++] = buffer[i]; 
				      readupdate(&i, &j, 1, &firstSector, &startingCluster);
				    }
				  save_b[n] = buffer[i];
				  readupdate(&i, &j, 1, &firstSector, &startingCluster);
				  deltatime = 0;
				  for(m = n; m >=0; m--) // calculate delta time
				    {
				      deltatime += (save_b[m]&0x7f) << 7*(n-m) ;
				    }
				  realtime=deltatime/(float)tick_per_qnote*(tempo/1000000.0);
				  realplay(realtime,note,0);
				  break;
				case 0xa0 :				
				case 0xb0 :			
				case 0xe0 :	
				  readupdate(&i, &j, 2, &firstSector, &startingCluster);
				  n = 0;
				  while(buffer[i] & 0x80) // get delta time bytes
				    {
				      readupdate(&i, &j, 1, &firstSector, &startingCluster);
				    }
				  readupdate(&i, &j, 1, &firstSector, &startingCluster);
				  break;
				case 0x90 :	
				  {
				    note = buffer[i];
				    readupdate(&i, &j, 1, &firstSector, &startingCluster);
				    velocity=buffer[i];
				    readupdate(&i, &j, 1, &firstSector, &startingCluster);
				    n = 0;
				    while(buffer[i] & 0x80) // get delta time bytes
				      {
					save_b[n++] = buffer[i]; 
					readupdate(&i, &j, 1, &firstSector, &startingCluster);
				      }
				    save_b[n] = buffer[i];
				    readupdate(&i, &j, 1, &firstSector, &startingCluster);
				    deltatime = 0;
				    for(m = n; m >=0; m--) // calculate delta time
				      {
					deltatime += (save_b[m]&0x7f) << 7*(n-m) ;
				      }
				    realtime=deltatime/(float)tick_per_qnote*(tempo/1000000.0);
				    
				    unsigned char temp=realplay(realtime,note,velocity);
				    if(temp==1)
				      goto go_to_label;
				    else if(temp==2)
				      {
					readupdate(&i, &j, trk_size-tki_p, &firstSector, &startingCluster);//fast forwart to the next track
					goto next_track;
				      }
				    
				    break;
				  }
				case 0xc0:
				case 0xd0:
				  readupdate(&i, &j, 1, &firstSector, &startingCluster);
				  n = 0;
				  while(buffer[i] & 0x80) // get delta time bytes
				    {
				      readupdate(&i, &j, 1, &firstSector, &startingCluster);
				    }
				  readupdate(&i, &j, 1, &firstSector, &startingCluster);
				  break;
				default:
				  sprintf(szBuffer,"Should not be here 1!");
				  LCD_command(CLEARSCR);
				  LCD_command(FIRST_ROW_START);
				  printf_strDM((unsigned char*)szBuffer);
				  LCD_command(SECOND_ROW_START);
				  sprintf(szBuffer,"Track %d",nt);
				  printf_strDM((unsigned char*)szBuffer);
				  while(1) //press confirm to continue
				    {
				      if(PINA==0xdf)
					{
					  while(PINA!=0xff);
					  debug=1;
					  break;
					}
				    }
				  break;
				}
			    }
			  else 
			    {	
			      switch (temp_command) 
				{
				case 0xf0:
				  readupdate(&i, &j, buffer[i]+1, &firstSector, &startingCluster);
				  n = 0;
				  while(buffer[i] & 0x80) // get delta time bytes
				    {
				      readupdate(&i, &j, 1, &firstSector, &startingCluster);
				    }
				  readupdate(&i, &j, 1, &firstSector, &startingCluster);
				  break;
				case 0xf1:
				case 0xf3:
				  readupdate(&i, &j, 1, &firstSector, &startingCluster);
				  n = 0;
				  while(buffer[i] & 0x80) // get delta time bytes
				    {
				      readupdate(&i, &j, 1, &firstSector, &startingCluster);
				    }
				  readupdate(&i, &j, 1, &firstSector, &startingCluster);
				  break;
				case 0xf2:
				  readupdate(&i, &j, 2, &firstSector, &startingCluster);
				  n = 0;
				  while(buffer[i] & 0x80) // get delta time bytes
				    {
				      readupdate(&i, &j, 1, &firstSector, &startingCluster);
				    }
				  readupdate(&i, &j, 1, &firstSector, &startingCluster);
				  break;
				default:
				  sprintf(szBuffer,"Should not be here 2!");
				  LCD_command(CLEARSCR);
				  LCD_command(FIRST_ROW_START);
				  printf_strDM((unsigned char*)szBuffer);
				  LCD_command(SECOND_ROW_START);
				  sprintf(szBuffer,"Track %d",nt);
				  printf_strDM((unsigned char*)szBuffer);
				  while(1) //press confirm to continue
				    {
				      if(PINA==0xdf)
					{
					  while(PINA!=0xff);
					  debug=1;
					  break;
					}
				    }
				  break;
				}
			    }
			  
			}
		      break;
		    }
		}
	      else // system event
		{
		  switch (buffer[i])
		    {
		    case 0xf0:
		      readupdate(&i, &j, 1, &firstSector, &startingCluster);
		      readupdate(&i, &j, buffer[i]+1, &firstSector, &startingCluster);
		      n = 0;
		      while(buffer[i] & 0x80) // get delta time bytes
			{
			  readupdate(&i, &j, 1, &firstSector, &startingCluster);
			}
		      readupdate(&i, &j, 1, &firstSector, &startingCluster);
		      break;
		    case 0xf1:
		    case 0xf3:
		      readupdate(&i, &j, 2, &firstSector, &startingCluster);
		      n = 0;
		      while(buffer[i] & 0x80) // get delta time bytes
			{
			  readupdate(&i, &j, 1, &firstSector, &startingCluster);
			}
		      readupdate(&i, &j, 1, &firstSector, &startingCluster);
		      break;
		    case 0xf2:
		      readupdate(&i, &j, 3, &firstSector, &startingCluster);
		      n = 0;
		      while(buffer[i] & 0x80) // get delta time bytes
			{
			  readupdate(&i, &j, 1, &firstSector, &startingCluster);
			}
		      readupdate(&i, &j, 1, &firstSector, &startingCluster);
		      break;
		    default:
		      readupdate(&i, &j, 1, &firstSector, &startingCluster);
		      n = 0;
		      while(buffer[i] & 0x80) // get delta time bytes
			{
			  readupdate(&i, &j, 1, &firstSector, &startingCluster);
			}
		      readupdate(&i, &j, 1, &firstSector, &startingCluster);
		      break;
		    }
		}
	    }
	  
	}

    next_track:
      readupdate(&i, &j, 4, &firstSector, &startingCluster);
      readupdate(&i, &j, 4, &firstSector, &startingCluster);
      
      trk_size = ((unsigned long)save_b[0]<<24)+((unsigned long)save_b[1]<<16)+((unsigned long)save_b[2]<<8)+save_b[3];
    }
 go_to_label:
  ;
}


//***************************************************************************************************
// Function: to call time interrupt to synthesize the sound 
//			 according to the note frequency and playtime and key control for PAUSE,BACK,NEXT TRACK
//	Argument: realtime--the real time of playing and non-playing time in second
//			  flag: 1 play 0 non-play
//	Return: flag of Key selecting: 1: for BACK key is pressed
//								   2: for NEXT TRACK key is pressed
//***************************************************************************************************
unsigned char realplay(float realtime, unsigned char note,unsigned char flag)
{	
  unsigned char rv=0;
  
  unsigned long total_count;
  if(flag)
    {
      total_count=Rounding(realtime*2*keys[note-21]);
      press_f=1;
      TIM16_WriteOCR1A((unsigned int)roundfloat(ocr[note-21]));
    }
  else
    {
      total_count=realtime*2000;
      press_f=0;
      TIM16_WriteOCR1A(CLK/(PSCALER*2000)-1);
    }
  time_count = 0;
  sei();			
  while (time_count<=total_count)
    {
      switch(PINA)
	{
	case 0xef://"BACK" button is pressed-->go back to Play File list
	  {
	    while(PINA!=0xff);//wait the button is release
	    rv=1;
	    cli();
	    return rv;
	  }
	case 0xdf://pause start
	  {
	    while(PINA!=0xff);
	    cli();//stop interupt
	    while(1)
	      {
		if(PINA==0xdf)//resume pause
		  {
		    while(PINA!=0xff);
		    sei();//start interupt
		    break;
		  }
	      }
	    break;
	  }
	  
	case 0x7f://next track
	  {
	    while(PINA!=0xff);
	    cli();
	    rv=2;
	    return rv;
	  }
	  
	default:
	  break;
	}
      
      ;
    }
  cli();
   
  return rv;
}


//***************************************************************************
//Function: if flag=READ then to read file from SD card and send contents to UART 
//if flag=VERIFY then functions will verify whether a specified file is already existing
//Arguments: flag (READ or VERIFY) and pointer to the file name
//return: 0, if normal operation or flag is READ or VERIFY
//	      1, if file is already existing and flag = VERIFY
//		  2, if file name is incompatible and flag = VERIFY or READ
//***************************************************************************
//will need the file name to execute this function
unsigned char readFile (unsigned char flag, unsigned char *fileName)
{
  struct dir_Structure *dir;
  unsigned long cluster, byteCounter = 0, fileSize, firstSector;
  unsigned int k;
  unsigned char j /*, error*/;
  
  //error = convertFileName (fileName); //convert fileName into FAT format
  //if(error) return 2; // filename is not compatible if flag = VERIFY or READ.
  
  dir = findFiles (GET_FILE, fileName, 2); //get the file location
  if(dir == 0)  
    return (0); // file does not exist if flag = VERIFY or READ.
  
if(flag == VERIFY) return (1);	//specified file name is already existing if flag  = VERIFY.
 
// rest of the function is to read data from the file.
 
 cluster = (((unsigned long) dir->firstClusterHI) << 16) | dir->firstClusterLO;
 
 fileSize = dir->fileSize;
 
  while(1)
    {
      firstSector = getFirstSector (cluster);
      
      for(j=0; j<sectorPerCluster; j++)
	{
	  SD_readSingleBlock(firstSector + j);
	  
	  for(k=0; k<512; k++)
	    {
	      //this is the place we use our data.
	       
	      if ((byteCounter++) >= fileSize ) return 0;
	    }
	}
      cluster = getSetNextCluster (cluster, GET, 0); // get next cluster from the chain
      //if cluster is end of chain, exit.
      if(cluster == 0) {display_back((PGM_P)(errorclutter),(PGM_P)(backreturn));; return 0;}
    }
  return 0;
}

//***************************************************************************
//Function: to search for the next free cluster in the root directory
//          starting from a specified cluster
//Arguments: Starting cluster
//return: the next free cluster

// This function actually first locate the corresponding FAT sector of the startCluster
//then start search from the beginning of this FAT sector for next free cluster.
//Not a very good idea because there might be free cluster before the startCluster.
//****************************************************************
unsigned long searchNextFreeCluster (unsigned long startCluster)
{
  unsigned long cluster, *value, sector;
  unsigned char i;
  
  startCluster -=  (startCluster % 128);   //to start with the first file in a FAT sector
  for(cluster =startCluster; cluster <totalClusters; cluster+=128) 
    {
      sector = unusedSectors + reservedSectorCount + ((cluster * 4) / bytesPerSector);
      SD_readSingleBlock(sector);
      for(i=0; i<128; i++)
	{
	  value = (unsigned long *) &buffer[i*4];
	  if(((*value) & 0x0fffffff) == 0)
            return(cluster+i);
	}  
    } 
  
  return 0;
}

//***************************************************************************
//Function: to display total memory and free memory of SD card, using UART
//Arguments: none
//return: none
//Note: this routine can take upto 15sec for 1GB card (@1MHz clock)
//it tries to read from SD whether a free cluster count is stored, if it is stored
//then it will return immediately. Otherwise it will count the total number of
//free clusters, which takes time
//****************************************************************************
void memoryStatistics (menu_t* mem)
{
  unsigned long freeClusters, totalClusterCount, cluster;
  unsigned long totalMemory, freeMemory;
  unsigned long sector, *value;
  unsigned int i;
  
  unsigned char totalm[20];
  unsigned char freem[20];
  totalMemory = totalClusters * sectorPerCluster / 1024;
  totalMemory *= bytesPerSector;
  
  freeMemory *= bytesPerSector ;
  
  mem[0].name.content=(unsigned char*)(PGM_P)totalmem;
  mem[0].name.type=0;
  mem[0].next=NULL;
  
  mem[1].name.content=totalm;
  mem[1].name.type=1;
  displayMemory (HIGH, totalMemory,mem[1].name.content);
  mem[1].next=NULL;
  freeClusters = getSetFreeCluster (TOTAL_FREE, GET, 0); 
  
  if(freeClusters > totalClusters)
{
  freeClusterCountUpdated = 0;
  freeClusters = 0;
  totalClusterCount = 0;
  cluster = rootCluster;    
  while(1)
    {
      sector = unusedSectors + reservedSectorCount + ((cluster * 4) / bytesPerSector) ;
      SD_readSingleBlock(sector);
      for(i=0; i<128; i++)
	{
	  value = (unsigned long *) &buffer[i*4];
	  if(((*value)& 0x0fffffff) == 0)
            freeClusters++;;
	  
	  totalClusterCount++;
	  if(totalClusterCount == (totalClusters+2)) break;
	}  
      if(i < 128) break;
      cluster+=128;
    } 
 }
  
  if(!freeClusterCountUpdated)
    getSetFreeCluster (TOTAL_FREE, SET, freeClusters); //update FSinfo next free cluster entry
  freeClusterCountUpdated = 1;  //set flag
  freeMemory = freeClusters * sectorPerCluster / 1024;
  freeMemory *= bytesPerSector ;
  mem[2].name.content=(unsigned char*)(PGM_P)(freemem);
  mem[2].name.type=0;
  mem[2].next=NULL;
    
  mem[3].name.content=freem;
  mem[3].name.type=1;
  displayMemory (HIGH, freeMemory,mem[3].name.content);
  mem[3].next=NULL;

  unsigned char number=4;
  //initialize the menu
  unsigned char current=0;
  uint8_t row=1;
  LCD_command(CLEARSCR);
  LCD_command(FIRST_ROW_START);
  printf_strPGM((PGM_P)(arrow));
  printf_str(mem[current].name);
  if(current+1<=number-1)
    {
      LCD_command(SECOND_ROW_START);
      printf_strPGM((PGM_P)(space));
      printf_str(mem[current+1].name);
    }
  
  //wait for input to change the menu
  DDRA=0x00; //set PORTA as input
  while(1)
    {
      switch(PINA)
	{
	case 0x7f://~0x80 sw7 is pushed down, go down
	  {
	    if(current!=number-1)//the current selection is not at the bottom of the menu
	      {
		if(row==1)//arrow in the first row then change it to point second row
		  {
		    row=2;
		    LCD_command(CLEARSCR);
		    printf_strPGM((PGM_P)(space));
		    printf_str(mem[current].name);
		    LCD_command(SECOND_ROW_START);
		    printf_strPGM((PGM_P)(arrow));
		    printf_str(mem[current+1].name);
		  }
		else //row=2->arrow in the second row then scroll the menu down on the screen
		  {
		    row=1;
		    LCD_command(CLEARSCR);
		    printf_strPGM((PGM_P)(arrow));
		    printf_str(mem[current+1].name);
		    if(current+2<=number-1)
		      {
			LCD_command(SECOND_ROW_START);
			printf_strPGM((PGM_P)(space));
			printf_str(mem[current+2].name);
		      }
		  }
		current++;
	      }
	    while(PINA!=0xff);//wait the button is released
	    break;
	  }
	  
	case 0xbf://~0x40 sw6 is push down, go up
	  {
	    if(current!=0)
	      {
		if(row=2)//arrow in the second row then change it to point the first row
		  {
		    row=1;
		    LCD_command(CLEARSCR);
		    printf_strPGM((PGM_P)(arrow));
		    printf_str(mem[current-1].name);
		    LCD_command(SECOND_ROW_START);
		    printf_strPGM((PGM_P)(space));
		    printf_str(mem[current].name);			
		  }
		else //row=1->arrow in the first row then scroll the menu up on the screen
		  {
		    row=2;
		    LCD_command(CLEARSCR);
		    LCD_command(SECOND_ROW_START);
		    printf_strPGM((PGM_P)(arrow));
		    printf_str(mem[current-1].name);
		    if(current-2>=0)
		      {
			LCD_command(FIRST_ROW_START);
			printf_strPGM((PGM_P)(space));
			printf_str(mem[current-2].name);
		      }
		  }
		current--;	
	      }
	    while(PINA!=0xff);//wait the button is released
	    break;
	  }
	  
	  
	case 0xef://~0x10 sw4 is pushed down-->go back
	  {
	    while(PINA!=0xff);//wait the button is released
	    
	    return;
	    break;
	  }
	  
	default:
	  break;
	  
	}//end of switch
    }//end of while(1)
}

//************************************************************
//Function: To convert the unsigned long value of memory into 
//          text string and send to UART
//Arguments: 1. unsigned char flag. If flag is HIGH, memory will be displayed in KBytes, else in Bytes. 
//			 2. unsigned long memory value
//return: none
//************************************************************
void displayMemory (unsigned char flag, unsigned long memory,unsigned char* memoryString)
{
  memoryString = (unsigned char*)strcpy_P((char*)memoryString,(char*)spacebytes); //19 character long string for memory display
  unsigned char i=0;
  
  for(i=12; i>0; i--) //converting freeMemory into ASCII string
    {
      if(i==5 || i==9) 
	{
	  memoryString[i-1] = ',';  
	  i--;
	}
      memoryString[i-1] = (memory % 10) | 0x30;
      memory /= 10;
      if(memory == 0) break;
    }
  if(flag == HIGH)  memoryString[13] = 'K';
}
//***************************************************************************************************
// Function: List File menu function to display the selected file attribute: filename and file size
// Argument: file--the selected file, memory file size in byte
//***************************************************************************************************
void disply_fileAttrb(unsigned char* file, unsigned long memory)
{
  unsigned char memoryString[20];
  strcpy_P((char*)memoryString,(char*)spacebytes); //19 character long string for memory display 
  unsigned char i=0;
  
  for(i=12; i>0; i--) //converting freeMemory into ASCII string
    {
      if(i==5 || i==9) 
	{
	  memoryString[i-1] = ',';  
	  i--;
	}
      memoryString[i-1] = (memory % 10) | 0x30;
      memory /= 10;
      if(memory == 0) break;
    }
  display_back(file,memoryString);
}




//********************************************************************
//Function: update the free memory count in the FSinfo sector. 
//			Whenever a file is deleted or created, this function will be called
//			to ADD or REMOVE clusters occupied by the file
//Arguments: #1.flag ADD or REMOVE #2.file size in Bytes
//return: none
//********************************************************************
void freeMemoryUpdate (unsigned char flag, unsigned long size)
{
  unsigned long freeClusters;
  //convert file size into number of clusters occupied
  if((size % 512) == 0) size = size / 512;//calculate number of sectors thie file occupied.
  else size = (size / 512) +1;
  if((size % 8) == 0) size = size / 8; //calculate number of clusters this file occupied.
  else size = (size / 8) +1;
  
  if(freeClusterCountUpdated)
    {
      freeClusters = getSetFreeCluster (TOTAL_FREE, GET, 0);
      if(flag == ADD)
	freeClusters = freeClusters + size;
      else  //when flag = REMOVE
	freeClusters = freeClusters - size;
      getSetFreeCluster (TOTAL_FREE, SET, freeClusters);
    }
}

//******** END ***********
