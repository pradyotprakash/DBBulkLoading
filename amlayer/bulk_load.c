# include <stdio.h>
# include "pf.h"
# include "am.h"
# include "time.h"
# include "testam.h"

/* Inserts a key into a leaf node */
InsertintoLeaf(fd,bagPage,pageBuf,attrLength,attrType,value,recId,buff_hits,buff_access)
int fd; // file descriptor
int attrLength;
char attrType;
int *bagPage;
char *pageBuf;/* buffer where the leaf page resides */
char *value;/* attribute value to be inserted*/
int recId;/* recid of the attribute to be inserted */
int *buff_hits;
int * buff_access;
{
	int recSize, numElts, attributeLength;
	AM_LEAFHEADER head, *header;
	header = &head;
	bcopy(pageBuf, (char*)header, AM_sl);
	
	attributeLength = header->attrLength;
	recSize = attributeLength + AM_si + AM_si + AM_ss; // value, single or multiple, pagenum, pointer within page
	numElts = header->numKeys;
	int ptr=recId;
	
	// inserting the first element
	if(numElts == 0){
		// assume that space is available since it's the first element
		
		header->keyPtr = header->keyPtr + recSize;
		header->numKeys = 1;
		
		// insert the key value into the node
		bcopy(value, pageBuf + AM_sl, attributeLength);
		
		//for now point to some random location as key = record value
		
		
		bcopy(&ptr, pageBuf + AM_sl + attributeLength, AM_si);
		// ptr = address of entry
		bcopy(&ptr, pageBuf + AM_sl + attributeLength + AM_si, AM_si);
		
		short single = 1;
		bcopy(&single, pageBuf + AM_sl + attributeLength + 2*AM_si, AM_ss);
		
		// write the header back onto the page
		bcopy(header, pageBuf, AM_sl);
		
		return TRUE;
	}
	// some elements already exist
	else{
		
		// get last element inserted
		int lastVal;
		
		bcopy((pageBuf + AM_sl + (numElts-1)*recSize), &lastVal, attributeLength);
		int tempval;
		bcopy(value, &tempval, attributeLength);
		
		// check if key is present
		if(tempval == lastVal){
			// yes, present
			
			short single;
			bcopy(pageBuf + AM_sl + (numElts-1)*recSize + attributeLength + 2*AM_si, &single, AM_ss);
			
			if(single == 0){
				// at least two similar values exist, no need to initialize stuff
				
				AM_BAGHEADER *bagHeader, bagHead;
				bagHeader = &bagHead;
				
				char *tempBuf;
				int pntr, bagPageNum, lastAddressinPage;
				
				// get the starting address within the correct page where the bag header resides
				bcopy((pageBuf + AM_sl + (numElts-1)*recSize + attributeLength), &bagPageNum, AM_si);
				bcopy((pageBuf + AM_sl + (numElts-1)*recSize + attributeLength + AM_si), &pntr, AM_si);
				
				PF_GetThisPage(fd, bagPageNum, &tempBuf, buff_hits);
				bcopy(tempBuf, &lastAddressinPage, AM_si);
				bcopy(tempBuf + pntr, bagHeader, AM_sb);
				
				// check if space is available in this node
				if(PF_PAGE_SIZE - lastAddressinPage >= AM_si){
					// add to the remaining elements
					bagHeader->numKeys++;
					bcopy(&ptr, tempBuf + bagHeader->lastPtr, AM_si);
					bagHeader->lastPtr += AM_si;
					lastAddressinPage += AM_si;
					
					bcopy(&lastAddressinPage, tempBuf, AM_si);
					bcopy(bagHeader, tempBuf + pntr, AM_sb);
					// PF_UnfixPage(fd, bagPageNum, TRUE);
					return TRUE;
				}
				else{
					// if not, then create a new node
					// chain the leaves
					
					PF_UnfixPage(fd, *bagPage, TRUE);
					AM_BAGHEADER *bagHeader1, bagHead1;
					char *tempBuf1;
					
					PF_AllocPage(fd, &bagPageNum, &tempBuf1);
					*bagPage = bagPageNum;
					bagHeader->nextPageNum = bagPageNum;
					bcopy(bagHeader, tempBuf + pntr, AM_sb);
					int s = AM_si + AM_sb + AM_si;
					bcopy(&s, tempBuf1, AM_si);
					
					bagHeader1 = &bagHead1;
					
					bagHeader1->numKeys = 1;
					bagHeader1->nextPageNum = -1;
					bagHeader1->lastPtr = s;
					bcopy(&ptr, tempBuf1+(s - AM_si), AM_si);
					bcopy(bagHeader1, tempBuf1 + AM_si, AM_sb);
					return TRUE;
				}
			}
			else{
				// second copy of the last value,
				// need to create a new header and bag to store the pointers
				
				char *tempBuf;
				int pntr, lastAddressinPage;
				PF_GetThisPage(fd, *bagPage, &tempBuf, buff_hits);
				
				// check is space is available in the page
				bcopy(tempBuf, &lastAddressinPage, AM_si);
				
				if(PF_PAGE_SIZE - lastAddressinPage < AM_sb + AM_si*2){
					
					// no space
					// allocate a new page and write stuff to it
					PF_UnfixPage(fd, *bagPage, TRUE);
					
					AM_BAGHEADER *bagHeader1, bagHead1;
					char *tempBuf1;
					int bagPageNum;
					
					PF_AllocPage(fd, &bagPageNum, &tempBuf1);
					*bagPage = bagPageNum;
					
					int pntr = AM_si + AM_sb + AM_si*2;
					bcopy(&pntr, tempBuf1, AM_si);
					
					bagHeader1 = &bagHead1;
					
					bagHeader1->numKeys = 2;
					bagHeader1->nextPageNum = -1;
					bagHeader1->lastPtr = pntr;
					
					int address1;
					// get the original address from the leaf node
					bcopy(pageBuf + AM_sl + (numElts-1) * recSize + attributeLength, &address1, AM_si);
					
					// update that value
					bcopy(&bagPageNum, pageBuf + AM_sl + (numElts-1)*recSize + attributeLength, AM_si);
					bcopy(&pntr, pageBuf + AM_sl + (numElts-1)*recSize + attributeLength + AM_si, AM_si);
					short single1 = 0;
					bcopy(&single1, pageBuf + AM_sl + (numElts-1)*recSize + attributeLength + 2*AM_si, AM_ss);
					
					// write values into tempBuf1
					bcopy(&pntr, tempBuf1, AM_si);
					bcopy(bagHeader1, tempBuf1 + AM_si, AM_sb);
					bcopy(&address1, tempBuf1 + AM_si + AM_sb, AM_si);
					bcopy(&ptr, tempBuf1 + AM_si + AM_sb + AM_si, AM_si);
					
					return TRUE;
				}
				else{
					
					// space available
					AM_BAGHEADER bagHead1, *bagHeader1;
					bagHeader1 = &bagHead1;
					
					bagHeader1->numKeys = 2;
					bagHeader1->nextPageNum = -1;
					bagHeader1->lastPtr = lastAddressinPage + AM_sb + AM_si*2;
					
					bcopy(bagHeader1, tempBuf + lastAddressinPage, AM_sb);
					
					int address1, pntr;
					pntr = lastAddressinPage;
					// get the original address from the leaf node
					bcopy(pageBuf + AM_sl + (numElts-1)*recSize + attributeLength, &address1, AM_si);
					
					// copy the two pointers
					bcopy(&address1, tempBuf + lastAddressinPage + AM_sb, AM_si);
					bcopy(&ptr, tempBuf + lastAddressinPage + AM_sb + AM_si, AM_si);
					
					lastAddressinPage = lastAddressinPage + AM_sb + AM_si*2;
					bcopy(&lastAddressinPage, tempBuf, AM_si);
					
					// update the leaf node
					
					bcopy(bagPage, pageBuf + AM_sl + (numElts-1)*recSize + attributeLength, AM_si);
					
					bcopy(&pntr, pageBuf + AM_sl + (numElts-1)*recSize + attributeLength + AM_si, AM_si);
					
					short single1 = 0;
					bcopy(&single1, pageBuf + AM_sl + (numElts-1)*recSize + attributeLength + 2*AM_si, AM_ss);
					
					return TRUE;
				}
			}
		}
		else{
			// element not present before
			// this element is being inserted for the first time
			if(PF_PAGE_SIZE - header->keyPtr < recSize){
				return FALSE;
			}
			else{
				// space is available
				header->keyPtr = header->keyPtr + recSize;
				header->numKeys++;
				
				int offset = AM_sl + numElts*recSize;
				// insert the key value into the node
				bcopy(value, pageBuf + offset, attributeLength);
				
				//for now point to some random location as key = record value
				// ptr = address of entry
				bcopy(&ptr, pageBuf + offset + attributeLength, AM_si);
				
				// this entry may or may not be there
				bcopy(&ptr, pageBuf + offset + attributeLength + AM_si, AM_si);
				
				short single = 1;
				bcopy(&single, pageBuf + offset + attributeLength + AM_si*2, AM_ss);
				
				// write the header back onto the page
				bcopy(header, pageBuf, AM_sl);
				
				return TRUE;
			}
		}
	}
}

AddtoParent(fileDesc,level,rightmost_pageNum,rightmost_buffer,value,attrLength,buff_hits,num_nodes,buff_access,length)
int fileDesc;
char **rightmost_buffer;
int * buff_access;
int * num_nodes;
char *value; /*  pointer to attribute value to be added -
gives back the attribute value to be added to it's parent*/
int level; /*Level at which the node which has to be added to parent is present*/
int *rightmost_pageNum;
int attrLength;
int *length; //Length of rightmost_pageNum and rightmost_buffer array
int * buff_hits;
{
	int errVal;
	char* parentPageBuf = rightmost_buffer[level+1];
	/*Page number of page to be added*/
	int pageNum = rightmost_pageNum[level];
	/*Page number of parent page to which pageNum has to be added*/
	int parentPageNum = rightmost_pageNum[level+1];
	/*Page buffer of parent page*/
	
	AM_INTHEADER head, *header;
	
	/*Initialise header */
	header = &head;
	
	/* copy the header from buffer */
	bcopy(parentPageBuf, header, AM_sint);
	int recSize = header->attrLength + AM_si;
	
	if(header->numKeys < header->maxKeys){
		AM_AddtoIntPage(parentPageBuf, value, pageNum, header, header->numKeys);
		/*copy updated header to parentPageBuf*/
		bcopy(header, parentPageBuf, AM_sint);
		// NEXT LINE PROBABLY NOT REQUIRED
		rightmost_buffer[level+1] = parentPageBuf;
		return (AME_OK);
	}
	else{
		int newPageNum;
		char* newPageBuf;
		errVal = PF_AllocPage(fileDesc, &newPageNum, &newPageBuf);
		AM_Check; //check if there is no error in the PF layer functionality
		AM_INTHEADER newhead, *newheader;
		newheader = &newhead;
		
		
		//NOTE: INCREMENT NUMBER OF NODES, IF MAINTAINED HERE
		(*num_nodes)++;
		
		/*Initialise newheader*/
		newheader->maxKeys = header->maxKeys;
		newheader->numKeys = 0;
		newheader->pageType = header->pageType;
		newheader->attrLength = header->attrLength;
		
		bcopy(newheader, newPageBuf, AM_sint);
		
		/*Right most key of parentPageBuf has to be deleted and put into the next node*/
		header->numKeys = header->numKeys - 1;
		bcopy(header, parentPageBuf, AM_sint);
		/*For putting the correct pointer (of the lower level node) into the new node on the right*/
		bcopy(parentPageBuf + AM_sint + recSize*(header->numKeys + 1), newPageBuf + AM_sint, AM_si);
		/*Add value to this newly created parent*/
		AM_AddtoIntPage(newPageBuf, value, pageNum, newheader, newheader->numKeys);
		bcopy(newheader, newPageBuf, AM_sint);
		
		
		if(parentPageNum == AM_RootPageNum){ // If a new root needs to be created
			/*Allocate new root page*/
			int newRootNum;
			char* newRootBuf;
			errVal = PF_AllocPage(fileDesc, &newRootNum, &newRootBuf);
			AM_Check;
			
			//NOTE: INCREMENT NUMBER OF NODES, IF MAINTAINED, HERE
			(*num_nodes)++;
			/*Fill in new root*/
			AM_FillRootPage(newRootBuf, parentPageNum, newPageNum, value, header->attrLength, header->maxKeys);
			
			/*Modify rightmost buf and num arrays*/
			(*length)++;
			
			AM_RootPageNum = newRootNum;
			
			rightmost_buffer[level+2] = newRootBuf;
			rightmost_pageNum[level+2] = AM_RootPageNum;
			
			
			/*level + 1 entries to be set to the newPageBuf and Num*/
			rightmost_buffer[level+1] = newPageBuf;
			rightmost_pageNum[level+1] = newPageNum;
			
			
			/*Unfix left sibling of new page or left child of new root*/
			errVal = PF_UnfixPage(fileDesc, parentPageNum, TRUE);
			AM_Check;
			return (AME_OK);
		}
		else{
			/*level + 1 entries to be set to the newPageBuf and Num*/
			rightmost_buffer[level+1] = newPageBuf;
			rightmost_pageNum[level+1] = newPageNum;
			
			/*Unfix left sibling of new page*/
			errVal = PF_UnfixPage(fileDesc, parentPageNum, TRUE);
			AM_Check;
			
			errVal = AddtoParent(fileDesc, level+1, rightmost_pageNum, rightmost_buffer, value, attrLength, buff_hits, num_nodes, buff_access, length);
			AM_Check;
		}
	}
	return (AME_OK);
}


InsertEntry(temp, fileDesc,attrType,attrLength,value,recId,last,buff_hits,num_nodes,buff_access)
int *temp; // current bag page number
int fileDesc; /* file Descriptor */
char *value; /* value to be inserted */
int recId; /* recId to be inserted */
char attrType; /* 'i' or 'c' or 'f' */
int attrLength; /* 4 for 'i' or 'f', 1-255 for 'c' */
int last; /*Wheteher the value to be inserted is the last value*/
int *buff_hits;
int * num_nodes;
int * buff_access;
{
	char *pageBuf; /* buffer to hold page */
	int pageNum; /* page number of the page in buffer */
	int errVal; /* return value of functions within this function */
	static int rightmost_pageNum[100000];
	static char *rightmost_buffer[100000];
	static int length=0;
	
	if(length==0){
		length++;
		errVal=PF_GetFirstPage(fileDesc,&pageNum,&pageBuf,buff_hits);
		
		AM_Check;
		rightmost_pageNum[0]=pageNum;
		rightmost_buffer[0]=pageBuf;
	}
	
	int isInserted= InsertintoLeaf(fileDesc, temp, rightmost_buffer[0],attrLength,attrType,value,recId,buff_hits,buff_access);
	
	if(isInserted==TRUE){
		//done
		
		if(last==1){
			PF_UnfixPage(fileDesc, *temp, TRUE);
			
			int i;
			for(i=0;i<length;i++)
			{
				errVal = PF_UnfixPage(fileDesc,rightmost_pageNum[i],TRUE);
				AM_Check;
			}
			
		}
	}
	else if(isInserted==FALSE){
		/* Create a new leafnode*/
		char *tempPageBuf, *tempPageBuf1; /*Stores the buffer location of newly allocated page*/
		int tempPageNum,tempPageNum1;/* Stores the page number of newly allocated page*/
		AM_LEAFHEADER temphead, *tempheader;//To store the header of newly allocated page
		
		tempheader = &temphead;
		
		errVal = PF_AllocPage(fileDesc,&tempPageNum,&tempPageBuf);
		
		AM_Check;
		(*num_nodes)++;
		
		/*Initialize the header of new leafnode*/
		tempheader=(AM_LEAFHEADER*)malloc(sizeof(AM_LEAFHEADER));
		tempheader->pageType = 'l';
		tempheader->recIdPtr = PF_PAGE_SIZE;
		tempheader->keyPtr = AM_sl;
		tempheader->freeListPtr = AM_NULL;
		tempheader->numKeys = 0;
		tempheader->numinfreeList = 0;
		tempheader->nextLeafPage = AM_NULL_PAGE;
		tempheader->attrLength = attrLength;
		tempheader->maxKeys = (PF_PAGE_SIZE - AM_sl)/(attrLength + 2*AM_si + AM_ss);
		
		/* copy the header onto the page */
		bcopy(tempheader,tempPageBuf,AM_sl);
		/* Make the next leaf page pointer of previous page point to the newly created page*/
		bcopy(rightmost_buffer[0],tempheader,AM_sl);
		tempheader->nextLeafPage = tempPageNum;
		bcopy(tempheader,rightmost_buffer[0],AM_sl);
		
		
		/*Insert the value to newly created leaf node*/
		InsertintoLeaf(fileDesc, temp, tempPageBuf,attrLength,attrType,value,recId,buff_hits,buff_access);
		
		if(length==1)
		{
			//Allocate new page for root
			errVal = PF_AllocPage(fileDesc,&tempPageNum1,&tempPageBuf1);
			AM_Check;
			//Assign global Vars
			AM_LeftPageNum = rightmost_pageNum[0];
			AM_RootPageNum=tempPageNum1;
			//Init Root
			AM_FillRootPage(tempPageBuf1,rightmost_pageNum[0],tempPageNum,value,
			tempheader->attrLength ,tempheader->maxKeys);
			length++;
			(*num_nodes)++;
			/*unfix the left most leaf node*/
			errVal = PF_UnfixPage(fileDesc,AM_LeftPageNum,TRUE);
			AM_Check;
			//Assign new leaf to rightmost[0], root to rightmost[1]
			rightmost_pageNum[0]=tempPageNum;
			rightmost_buffer[0]=tempPageBuf;
			rightmost_pageNum[1]=tempPageNum1;
			rightmost_buffer[1]=tempPageBuf1;
			if(last==1)
			{
				PF_UnfixPage(fileDesc, *temp, TRUE);
				//Unfix all pages
				for(int i=0;i<length;i++)
				{
					
					errVal = PF_UnfixPage(fileDesc,rightmost_pageNum[i],TRUE);
					AM_Check;
				}
				AM_EmptyStack();
				return(AME_OK);
			}
		}
		else
		{
			//Root node already present
			errVal = PF_UnfixPage(fileDesc,rightmost_pageNum[0],TRUE);
			AM_Check;
			rightmost_pageNum[0]=tempPageNum;
			rightmost_buffer[0]=tempPageBuf;
			errVal=AddtoParent(fileDesc,0,rightmost_pageNum,rightmost_buffer,value,attrLength,buff_hits,num_nodes,buff_access, &length);
			
			if(last==1)
			{
				PF_UnfixPage(fileDesc, *temp, TRUE);
				//TODO Redistribute
				for(int i=0;i<length;i++)
				{
					
					errVal = PF_UnfixPage(fileDesc,rightmost_pageNum[i],TRUE);
					AM_Check;
				}
			}
		}
	}
	if (errVal < 0)
	{
		AM_EmptyStack();
		AM_Errno = errVal;
		return(errVal);
	}
	AM_EmptyStack();
	return(AME_OK);
}

void file_load_nosort(char* filename){
	char fname[50];
	PF_Init();
	
	AM_CreateIndex(RELNAME,0,INT_TYPE,sizeof(int));
	
	sprintf(fname,"%s.0",RELNAME);
	int fd = PF_OpenFile(fname);
	
	int *num_nodes, *buff_hits, *buff_access;
	num_nodes = (int *)malloc(sizeof(int));
	buff_hits = (int *)malloc(sizeof(int));
	buff_access = (int *)malloc(sizeof(int));
	(*num_nodes) = 1;
	(*buff_hits) = 0;
	(*buff_access) = 1;
	
	time_t start, end;
	
	start = clock();
	FILE *fp;
	int temp;
	int count=0;
	fp = fopen(filename, "r");
	while(!feof(fp)){
		count++;
		fscanf(fp, "%d", &temp);
		AM_InsertEntry(fd,INT_TYPE,sizeof(int),(char *)&temp,temp,buff_hits,num_nodes,buff_access);
	}
	temp++;
	count++;
	AM_InsertEntry(fd,INT_TYPE,sizeof(int),(char *)&temp,temp,buff_hits,num_nodes,buff_access);
	end = clock();
	
	// Number of keys
	printf("%i & ", count);
	// PF_PAGE_SIZE
	printf("%i & ", PF_PAGE_SIZE);
	// Number of nodes created
	printf("%i & ", (*num_nodes));
	// Number of Read Access
	printf("%i & ", nReadTransfers);
	// Number of Write Access
	printf("%i & ", nWriteTransfers);
	// access times
	printf("%G & ", 1e3*(end - start)/CLOCKS_PER_SEC);
	
	PF_CloseFile(fd);
	AM_DestroyIndex(RELNAME,0);
	
}

void file_load_sort(char* filename){
	
	char fname[50];
	// PF_Init();
	
	AM_CreateIndex(RELNAME,0,INT_TYPE,sizeof(int));
	sprintf(fname,"%s.2",RELNAME);
	int fd = PF_OpenFile(fname);
	
	int num_nodes, buff_hits, buff_access;
	num_nodes = (int *)malloc(sizeof(int));
	buff_hits = (int *)malloc(sizeof(int));
	buff_access = (int *)malloc(sizeof(int));
	(num_nodes) = 1;
	(buff_hits) = 0;
	(buff_access) = 1;
	time_t start, end;
	
	start = clock();
	char cmd[] = "sort -f -n ";
	strcat(cmd, filename);
	strcat(cmd, " > temp_out");
	system(cmd);
	
	FILE *fp;
	int temp;
	fp = fopen("temp_out", "r");
	int count=0;
	while(!feof(fp)){
		count++;
		fscanf(fp, "%d", &temp);
		AM_InsertEntry(fd,INT_TYPE,sizeof(int),(char *)&temp,temp,&buff_hits,&num_nodes,&buff_access);
	}
	temp++;
	count++;
	AM_InsertEntry(fd,INT_TYPE,sizeof(int),(char *)&temp,temp,&buff_hits,&num_nodes,&buff_access);
	
	end = clock();
	// Number of nodes created
	printf("%i & ", (num_nodes));
	// Number of Read Access
	printf("%i & ", nReadTransfers);
	// Number of Write Access
	printf("%i & ", nWriteTransfers);
	// access times
	printf("%G & ", 1e3*(end - start)/CLOCKS_PER_SEC);
	
	PF_CloseFile(fd);
	AM_DestroyIndex(RELNAME,0);
	
}

main(int argc, int **argv)
{
	int fd;    /* file descriptor for the index */
	char fname[80];    /* file name */
	char buf[250];    /* buf to store data to be inserted into index */
	int recnum;    /* record number */
	int sd;    /* scan descriptor */
	int numrec;    /* # of records retrieved */
	int testval;
	int *num_nodes, *buff_hits, *buff_access;
	num_nodes = (int *)malloc(sizeof(int));
	buff_hits = (int *)malloc(sizeof(int));
	buff_access = (int *)malloc(sizeof(int));
	(*num_nodes) = 1;
	(*buff_hits) = 0;
	(*buff_access) = 1;
	
	/* init */
	PF_Init();
	char* filename = argv[1];
	
	file_load_nosort(filename);
	file_load_sort(filename);
	
	time_t start, end;
	
	/* create index */
	AM_CreateIndex(RELNAME,0,INT_TYPE,sizeof(int));
	
	/* open the index */
	sprintf(fname,"%s.0",RELNAME);
	
	fd = PF_OpenFile(fname);
	
	//Allocate first bag page
	int temp, temp1, count = 0;
	char *temp_buffer;
	
	start = clock();
	PF_AllocPage(fd, &temp, &temp_buffer);
	int size_int = AM_si;
	bcopy(&size_int, temp_buffer, AM_si);
	
	
	char cmd[] = "sort -f -n ";
	strcat(cmd, filename);
	strcat(cmd, " > temp_out");
	system(cmd);
	
	FILE *fp;
	
	fp = fopen("temp_out", "r");
	
	while(!feof(fp)){
		fscanf(fp, "%d", &temp1);
		InsertEntry(&temp,fd,INT_TYPE,sizeof(int),(char *)&temp1,temp1,0,buff_hits,num_nodes,buff_access);
		++count;
	}
	// for(temp1 = 0;temp1<100;++temp1){
	// 	InsertEntry(&temp,fd,INT_TYPE,sizeof(int),(char *)&temp1,temp1,0,buff_hits,num_nodes,buff_access);
	// }
	temp1++;
	InsertEntry(&temp,fd,INT_TYPE,sizeof(int),(char *)&(temp1),temp1,1,buff_hits,num_nodes,buff_access);
	++count;
	
	end = clock();
	
	// OurPrintTree(fd, AM_RootPageNum, 'i');
	
	// Number of nodes created
	printf("%i & ", (*num_nodes));
	// Number of Read Access
	printf("%i & ", nReadTransfers);
	// Number of Write Access
	printf("%i & ", nWriteTransfers);
	// access times
	printf("%G\n", 1e3*(end - start)/CLOCKS_PER_SEC);
	
	xPF_CloseFile(fd);
}