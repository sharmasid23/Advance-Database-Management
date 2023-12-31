#include<stdio.h>
#include<stdlib.h>
#include "buffer_mgr.h"
#include "storage_mgr.h"
#include <math.h>

int bufferPoolMax;
int rIndex;
int writeCnt;
int totoalHits;

typedef struct Page
{
	SM_PageHandle data;
	PageNumber pageNum;
	int isDirty;
	int currCnt;
	int hitCnt;
	int freqCnt;
} PageF;

int bufferPoolMax = 0;
int rIndex = 0;
int writeCnt = 0;
int totoalHits = 0;

//Written by Avadhoot Kodag
extern RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName,const int numPages, ReplacementStrategy strategy, void *stratData){
	bufferPoolMax = 0;
	rIndex = 0;
	writeCnt = 0;
	totoalHits = 0;

	PageF *pages = malloc(sizeof(PageF) * numPages);
	bm->strategy = strategy;
	bm->mgmtData = pages;
	bm->pageFile = (char *)pageFileName;

	int i = 0;
	while(i < numPages){
		pages[i].data = NULL;
		pages[i].pageNum = -1;
		pages[i].isDirty = pages[i].currCnt = pages[i].hitCnt =  pages[i].freqCnt = 0;
		i++;
	}
	bm->numPages = bufferPoolMax = numPages;
	return RC_OK;
}

//Written by Siddharth Sharma
//extern RC shutdownBufferPool(BM_BufferPool *const bm)
//{
//	PageF *pFrame = (PageF *)bm->mgmtData;
//	forceFlushPool(bm);
//	free(pFrame);
//	bm->mgmtData = NULL;
//	return RC_OK;
//}

extern RC shutdownBufferPool(BM_BufferPool *const bm)
{
	PageF *pageFrame = (PageF *)bm->mgmtData;
	// Write all dirty pages (modified pages) back to disk
	forceFlushPool(bm);

	int i;
	for(i = 0; i < bufferPoolMax; i++)
	{
		// If fixCount != 0, it means that the contents of the page was modified by some client and has not been written back to disk.
		if(pageFrame[i].currCnt != 0)
		{
			return RC_PINNED_PAGES_IN_BUFFER;
		}
	}

	// Releasing space occupied by the page
	free(pageFrame);
	bm->mgmtData = NULL;
	return RC_OK;
}

//Written by Avadhoot Kodag
extern RC forceFlushPool(BM_BufferPool *const bm)
{
	PageF *pFrame = (PageF *)bm->mgmtData;
	SM_FileHandle fh;
	if(pFrame!= NULL){
		int k = 0;
		while(k<bufferPoolMax){
			if(pFrame[k].isDirty == 1){
				if(pFrame[k].currCnt == 0){
					openPageFile(bm->pageFile, &fh);
					writeBlock(pFrame[k].pageNum, &fh, pFrame[k].data);
					writeCnt += 1;
					pFrame[k].isDirty = 0;
				}
			}
			k += 1;
		}
		return RC_OK;
	}
	else{
		return RC_ERROR_IN_FLUSH_POOL;
	}
}

//Written by Siddharth Sharma
extern RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	PageF *pFrame = (PageF *)bm->mgmtData;
	int pageNum = page->pageNum;
	if(pFrame != NULL){
		int j = 0;
		while(j < bufferPoolMax)
		{
			if(pFrame[j].pageNum == pageNum)
			{
				pFrame[j].isDirty = 1;
				return RC_OK;
			}
			j+=1;
		}
		return RC_ERROR;
	}
	else{
		return RC_ERROR;
	}
}

//Written by Avadhoot Kodag
extern RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	PageF *pFrame = (PageF *)bm->mgmtData;
	int pageNumPointer = page->pageNum;

	int i = 0;
	while(i < bufferPoolMax)
	{
		if(pFrame[i].pageNum == pageNumPointer)
		{
			pFrame[i].currCnt-=1;
			break;
		}
		i+=1;
	}
	return RC_OK;
}



//Written by Siddharth Sharma
extern RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	PageF *pFrame = (PageF *)bm->mgmtData;
	SM_FileHandle fh;
	int pageNumPointer = page->pageNum;
	int i = 0;
	while(i < bufferPoolMax){
		if(pFrame[i].pageNum == pageNumPointer)
		{
			if (openPageFile(bm->pageFile, &fh) == 0){
				if (writeBlock(pFrame[i].pageNum, &fh, pFrame[i].data)==0){
					pFrame[i].isDirty = 0;
					writeCnt += 1;
				}
			}
		}
		i += 1;
	}
	return RC_OK;
}

//Written by Siddharth Sharma
extern void FIFO(BM_BufferPool *const bm, PageF *page)
{
	PageF *pFrame = (PageF *) bm->mgmtData;
	int i = 0;
	int fIndex = rIndex % bufferPoolMax;
	int fibm =fIndex % bufferPoolMax;
	SM_FileHandle fh;

	while(i < bufferPoolMax)
	{
		if(pFrame[fIndex].currCnt != 0)
		{
			fIndex+=1;
			fIndex = (fibm == 0) ? 0 : fIndex;
		}
		else
		{
			if(pFrame[fIndex].isDirty == 1)
				{
					if(openPageFile(bm->pageFile, &fh)== 0)
					{
						if (writeBlock(pFrame[fIndex].pageNum, &fh, pFrame[fIndex].data)==0)
						{
							writeCnt+=1;
						}
					}
				}
				pFrame[fIndex].data = page->data;
				pFrame[fIndex].pageNum = page->pageNum;
				pFrame[fIndex].isDirty = page->isDirty;
				pFrame[fIndex].currCnt = page->currCnt;
				break;
		}
		i+=0;
	}
}

//Written by Avadhoot Kodag
extern void LRU(BM_BufferPool *const bm, PageF *page)
{
	PageF *pFrame = (PageF *) bm->mgmtData;
	int i = 0;
	int LHIndex, LHNum;

	while(i < bufferPoolMax){
		if(pFrame[i].currCnt == 0)
		{
			LHIndex = i;
			LHNum = pFrame[i].hitCnt;
			break;
		}
		i+=1;
	}


	i = LHIndex + 1;
	while(i < bufferPoolMax){
		if(pFrame[i].hitCnt < LHNum)
		{
			LHIndex = i;
			LHNum = pFrame[i].hitCnt;
		}
		i+=1;
	}

	if(pFrame[LHIndex].isDirty == 1)
	{
		SM_FileHandle fh;
		if(openPageFile(bm->pageFile, &fh)== 0){
			if(writeBlock(pFrame[LHIndex].pageNum, &fh, pFrame[LHIndex].data) == 0) {
				writeCnt++;
			}
		}
	}

	pFrame[LHIndex].data = page->data;
	pFrame[LHIndex].isDirty = page->isDirty;
	pFrame[LHIndex].currCnt = page->currCnt;
	pFrame[LHIndex].pageNum = page->pageNum;
	pFrame[LHIndex].hitCnt = page->hitCnt;
}

//Written by Avadhoot Kodag
extern RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page,
	    const PageNumber pageNum)
{
	bool isFull = true;
	PageF *pageF = (PageF *)bm->mgmtData;
	SM_FileHandle fh;
	if(pageF[0].pageNum == -1)
	{
		if (openPageFile(bm->pageFile, &fh) == 0){
			pageF[0].data = (SM_PageHandle) malloc(PAGE_SIZE);
			if(ensureCapacity(pageNum,&fh) == 0){
				if(readBlock(pageNum, &fh, pageF[0].data) == 0){
					page->pageNum = pageNum;
					page->data = pageF[0].data;
					pageF[0].pageNum = pageNum;
					pageF[0].currCnt += 1;
					pageF[0].hitCnt = 0;
					pageF[0].freqCnt = 0;
					rIndex = 0;
					totoalHits = 0;
					return RC_OK;
				}
				else{
					return RC_READ_NON_EXISTING_PAGE;
				}
			}
			else{
				return RC_ERROR_IN_ENSURE_CAPACITY;
			}
		}
		else{
			return RC_ERROR_IN_OPEN_PAGEFILE;
		}

	}
	else
	{
		int m = 0;
		while(m < bufferPoolMax){
			if(pageF[m].pageNum != -1)
			{
				if(pageF[m].pageNum == pageNum)
				{
					totoalHits += 1;
					if(bm->strategy == RS_LRU)
						pageF[m].hitCnt = totoalHits;
					pageF[m].currCnt += 1;
					isFull = false;
					page->pageNum = pageNum;
					page->data = pageF[m].data;
					return RC_OK;
				}
			}
			else {
				if (openPageFile(bm->pageFile, &fh) == 0){
				pageF[m].data = (SM_PageHandle) malloc(PAGE_SIZE);
				if(readBlock(pageNum, &fh, pageF[m].data) == 0){
					page->pageNum = pageNum;
					pageF[m].pageNum = pageNum;
					rIndex += 1;
					totoalHits += 1;

					if(bm->strategy == RS_LRU)
						pageF[m].hitCnt = totoalHits;

					page->data = pageF[m].data;
					isFull = false;
					pageF[m].currCnt = 1;
					pageF[m].freqCnt = 0;
					return RC_OK;
				}
				else{
					return RC_READ_NON_EXISTING_PAGE;
				}
			}
			else{
				return RC_ERROR_IN_OPEN_PAGEFILE;
			}
		}
		m+=1;
	}
		if(isFull == true)
		{
			PageF *newPage = (PageF *) malloc(sizeof(PageF));
			if(openPageFile(bm->pageFile, &fh) == 0){
				newPage->data = (SM_PageHandle) malloc(PAGE_SIZE);
				if(readBlock(pageNum, &fh, newPage->data) == 0){
					newPage->pageNum = pageNum;
					newPage->isDirty = newPage->freqCnt = 0;
					newPage->currCnt = 1;
					rIndex += 1;
					totoalHits += 1;
					page->pageNum = pageNum;
					page->data = newPage->data;

					if(bm->strategy == RS_LRU)
						newPage->hitCnt = totoalHits;

					switch(bm->strategy)
					{
						case RS_FIFO:
							FIFO(bm, newPage);
							break;

						case RS_LRU:
							LRU(bm, newPage);
							break;

						default:
							printf("\nAlgorithm Not Implemented\n");
							break;
					}
				}
				else{
					return RC_READ_NON_EXISTING_PAGE;
				}
			}
			else{
				return RC_ERROR_IN_OPEN_PAGEFILE;
			}
		}
		return RC_OK;
	}
}

//Written by Vaishnavi Mule
extern PageNumber *getFrameContents (BM_BufferPool *const bm)
{
	int i = 0;
	PageNumber *fData = malloc(sizeof(PageNumber) * bufferPoolMax);
	PageF *pFrame = (PageF *) bm->mgmtData;
	while(i<bufferPoolMax)
	{
		if(pFrame[i].pageNum != -1){
			fData[i] = pFrame[i].pageNum;
		}
		else{
			fData[i] = NO_PAGE;
		}
		i+=1;
	}
	return fData;
}

//Written by Vaishnavi Mule
extern bool *getDirtyFlags (BM_BufferPool *const bm)
{
	int i = 0;
	bool *Flags = malloc(sizeof(bool) * bufferPoolMax);
	PageF *pFrame = (PageF *)bm->mgmtData;
	while(i < bufferPoolMax)
	{
		if(pFrame[i].isDirty == 1)
			Flags[i] = true;
		else
			Flags[i] = false;
		i+=1;
	}	
	return Flags;
}

//Written by Vaishnavi Mule
extern int *getFixCounts (BM_BufferPool *const bm)
{
	int i = 0;
	int *fixCounts = malloc(sizeof(int) * bufferPoolMax);
	PageF *pFrame= (PageF *)bm->mgmtData;
	while(i < bufferPoolMax)
	{
		if(pFrame[i].currCnt != -1)
			fixCounts[i] = pFrame[i].currCnt;
		else
			fixCounts[i] = 0;
		i+=1;
	}	
	return fixCounts;
}

//Written by Vaishnavi Mule
extern int getNumReadIO (BM_BufferPool *const bm)
{
	return (rIndex + 1);
}

//Written by Vaishnavi Mule
extern int getNumWriteIO (BM_BufferPool *const bm)
{
	return writeCnt;
}
