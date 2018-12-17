/*
 * Message Channel Exit - Sniffer (File Logger) for IBM WebSphere MQ
 *
 * Copyright (C) 2015, Invenire Aude, Albert Krzymowski
 *
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <cmqc.h>
#include <regex.h>

#include <chlf.h>

/**********************************************************************************/
#define IAE_PATTERN_LEN                    MQ_Q_NAME_LENGTH*4
#define IAE_FILENAME_LEN                   256
#define IAE_TIME_LEN                       64

#define IEA_ERR_BUFFER						1024
#define IAE_DATASEARCH_LEN					4096
#define IAE_MAXMSGS                         99999999
/**********************************************************************************/

typedef enum DataFormat{
	DF_QLOAD = 1,
	DF_DATA  = 2
}DataFormat;

typedef struct {

	DataFormat iDataFormat;

	int    iOffset;
	int    iNumMsgs;

	MQCHAR sQueueName[MQ_Q_NAME_LENGTH + 1];
	MQCHAR sQMgrName[MQ_Q_MGR_NAME_LENGTH + 1];

	MQCHAR sQueuePattern[IAE_PATTERN_LEN + 1];
	MQCHAR sQMgrPattern[IAE_PATTERN_LEN + 1];

	char   sInputFile[IAE_FILENAME_LEN + 1];
	char   sOutputFile[IAE_FILENAME_LEN + 1];

	char   sDataSearch[IAE_DATASEARCH_LEN];
	char   sDataPattern[IAE_DATASEARCH_LEN];

	int    bOverwrite;


	regex_t reQueue;
	regex_t reQMgr;
	regex_t reData;


}Options;

/**********************************************************************************/
Options TheOptions = {
		DF_QLOAD,
		0,
		IAE_MAXMSGS,
		"", "",
		"", "",
		"", "",
		"", "",
		0,
		0
};

/**********************************************************************************/
void usage(int iCode){

	fprintf(stderr,"\n  Usage: iae_cfe_getdata [options] <input_file> \n");

	fprintf(stderr,"\n\tOptions: \n");

	fprintf(stderr,"\t\t -h            This help.\n");
	fprintf(stderr,"\t\t -o <file>     Output file name.\n");
	fprintf(stderr,"\t\t -O <file>     Output file name (overwrite).\n");
	fprintf(stderr,"\t\t -s <string>   Search text.\n");
	fprintf(stderr,"\t\t -S <pattern>  Search text as regex pattern.\n");
	fprintf(stderr,"\t\t -m <string>   Destination queue manager name.\n");
	fprintf(stderr,"\t\t -M <string>   Destination queue manager name as regex pattern.\n");
	fprintf(stderr,"\t\t -q <string>   Destination queue name.\n");
	fprintf(stderr,"\t\t -Q <string>   Destination queue name as regex pattern.\n");
	fprintf(stderr,"\t\t -n <num>      Number of messages to extract.\n");
	fprintf(stderr,"\t\t -N <num>      Skip first <num> messages.\n");
	fprintf(stderr,"\t\t -D <data_format>   Data format, one of: .\n");
	fprintf(stderr,"\t\t                     1 - qload compatible (default) .\n");
	fprintf(stderr,"\t\t                     2 - pure data only.\n");

	fprintf(stderr,"\n");
	exit(iCode);
}
/**********************************************************************************/
static int compile_regex (regex_t *r, const char * regex_text)
{
    int status = regcomp (r, regex_text, REG_EXTENDED|REG_NEWLINE);
    if (status != 0) {
	char error_message[IEA_ERR_BUFFER];
	regerror (status, r, error_message, IEA_ERR_BUFFER);
        printf ("Regex error compiling '%s': %s\n",
                 regex_text, error_message);
        return 1;
    }
    return 0;
}
/********************************************************************/
static void mqstrcpy(char *d, const MQCHAR *s, int n){

	while(n-- && *s && *s != ' '){

		 char c = (char) *s++;

		 if(isalpha(c) || isdigit(c))
			 *d++=c;
		 else
			 *d++='_';

	}

	*d=0;
}
/**********************************************************************************/
int matchMQString(regex_t *r, const char *s, int n){

	char sBuffer[n+1];
	int  rc = 0;

	mqstrcpy(sBuffer,s,n);

	rc = regexec(r, sBuffer, 0, NULL, 0);
	if (!rc) {
	    return 1;
	}
	else if (rc == REG_NOMATCH) {
	    return 0;
	}
	else {
		char error_message[IEA_ERR_BUFFER];
	    regerror(rc, r, error_message, IEA_ERR_BUFFER);
	    fprintf(stderr, "Regex match failed: %s\n", error_message);
	   return -1;
	}
}
/**********************************************************************************/
void iaestrncpy(char *d, const char *s, int n){

	while( n-- > 1 && *s)
		*d++=*s++;
	*d=0;
}
/**********************************************************************************/
void readOptions(int argc, char *argv[]){

	  int index;
	  int c;

	while ((c = getopt(argc, argv, "n:N:D:O:o:s:S:m:M:q:Q:h")) != -1)
		switch (c) {
		case 'm':
			iaestrncpy(TheOptions.sQMgrName, optarg, MQ_Q_MGR_NAME_LENGTH + 1);
			break;

		case 'M':
			iaestrncpy(TheOptions.sQMgrPattern, optarg, IAE_PATTERN_LEN + 1);
			break;

		case 'q':
			iaestrncpy(TheOptions.sQueueName, optarg, MQ_Q_NAME_LENGTH + 1);
			break;

		case 'Q':
			iaestrncpy(TheOptions.sQMgrPattern, optarg, MQ_Q_MGR_NAME_LENGTH + 1);
			break;

		case 's':
			iaestrncpy(TheOptions.sDataSearch, optarg, IAE_DATASEARCH_LEN + 1);
			break;

		case 'S':
			iaestrncpy(TheOptions.sDataPattern, optarg, IAE_DATASEARCH_LEN + 1);
			break;

		case 'o':
			iaestrncpy(TheOptions.sOutputFile, optarg, IAE_FILENAME_LEN + 1);
			break;

		case 'N':
			TheOptions.iOffset = strtol(optarg, 0, 10);
			if((errno != 0 && TheOptions.iOffset == 0) || TheOptions.iOffset < 0 || TheOptions.iOffset > 9999999){
				fprintf(stderr," Bad value for the -N option.\n");
				usage(1);
			}

			break;

		case 'n':
			TheOptions.iNumMsgs = strtol(optarg, 0, 10);
			if((errno != 0 && TheOptions.iNumMsgs == 0) || TheOptions.iNumMsgs < 0 || TheOptions.iNumMsgs > 9999999){
				fprintf(stderr," Bad value for the -n option.\n");
				usage(1);
			}
			break;

		case 'O':
			TheOptions.bOverwrite = 1;
			iaestrncpy(TheOptions.sOutputFile, optarg, IAE_FILENAME_LEN + 1);
			break;

		case 'D':
			TheOptions.iDataFormat = *optarg - '0';
			break;

		case 'h':
			usage(0);
			break;

		case '?':
			if (isprint(optopt))
				fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			else
				fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
		usage(1);
		break;
	default:
		usage(1);
		}

		if(*TheOptions.sQMgrName && *TheOptions.sQMgrPattern){
			fprintf(stderr," Cannot specify both: queue manager name and pattern.\n");
			usage(1);
		}

		if(*TheOptions.sQueueName && *TheOptions.sQueuePattern){
			fprintf(stderr," Cannot specify both: queue name and pattern.\n");
			usage(1);
		}

		if(*TheOptions.sDataSearch && *TheOptions.sDataPattern){
			fprintf(stderr," Cannot specify both: data search and pattern.\n");
			usage(1);
		}

		if(*TheOptions.sQueuePattern)
			compile_regex(&TheOptions.reQueue,TheOptions.sQueuePattern);

		if(*TheOptions.sQMgrPattern)
			compile_regex(&TheOptions.reQMgr,TheOptions.sQMgrPattern);

		if(*TheOptions.sDataPattern)
				compile_regex(&TheOptions.reData,TheOptions.sDataPattern);

		if(optind + 1 != argc){
			fprintf(stderr," Specify exactly one input file name, please.\n");
			usage(1);
		}


		if(TheOptions.iDataFormat != DF_DATA &&
		   TheOptions.iDataFormat != DF_QLOAD){
			fprintf(stderr, " Bad data format requested (-D option).\n");
			usage(1);
		}

		strncpy(TheOptions.sInputFile, argv[optind], IAE_FILENAME_LEN);

}
/**********************************************************************************/
int openInputFile(){

	int fd = open(TheOptions.sInputFile, O_RDONLY);

	if(fd == -1){
		perror("Problem opening the input file");
		exit(1);
	}

	return fd;
}
/**********************************************************************************/
void closeInputFile(int fd){
	if(fd >= 0)
		close(fd);
}
/**********************************************************************************/
FILE *openOutputStream(){

	if(!*TheOptions.sOutputFile)
		return stdout;

	FILE* f=fopen(TheOptions.sOutputFile, TheOptions.bOverwrite ? "w" : "a" );

	if(f == NULL){
		if(errno == EEXIST)
			fprintf(stderr,"The output file exists, specify the -O option to force overwrite.");
		else
			perror("Problem when opening the output stream");
	}
	return f;
}
/**********************************************************************************/
void closeOutputStream(FILE *fOut){
	if(fOut != stdout)
		fclose(fOut);
}

/**********************************************************************************/
int readBytes(int iIn, char* pBuffer, int iDataLen){

	while(iDataLen > 0){

		int iRead = read(iIn, pBuffer, iDataLen);

		if(iRead == -1){
			perror("Error when reading data");
			return -1;
		}

		if(iRead == 0)
			return 0;

		pBuffer  += iRead;
		iDataLen -= iRead;
	}

	return 1;
};
/************************************************************************/
static const char * _hexTab = "0123456789ABCDEF";

static void printHex(FILE *f, const char* sPattern, void *pData, int iDataLen){

	char sBuffer[256];
	char *sTmp=sBuffer;

	if(iDataLen > 128){
		perror("Internal Error in makeHex()");
		exit(1);
	}

	unsigned char *sData = (unsigned char*)pData;

	while(iDataLen--){
		*sTmp++ = _hexTab[ ((*sData) >> 4) & 0x0f ];
		*sTmp++ = _hexTab[ (*sData) & 0x0f ];
		sData++;
	}

	*sTmp++ = 0;

	fprintf(f,sPattern,sBuffer);
}
/**********************************************************************************/

typedef struct {

	IAE_CHFL_Header header;

	PMQCHAR pData;
	MQLONG  iBufferLen;
}Buffer;

Buffer TheBuffer = { 0, 0 };
/**********************************************************************************/
static void printData(FILE* fOut, Buffer *pBuffer){

	PMQCHAR  sData       = pBuffer->pData;
	int      iDataLength = pBuffer->header.iDataLength;

	MQXQH* pMQXQH = (MQXQH*) sData;
	MQMD1* pMQMD = (MQMD1*) &(pMQXQH->MsgDesc);

	char         *sFormat = pMQMD->Format;

	iDataLength  -= sizeof(MQXQH);
	sData        += sizeof(MQXQH);

	while(1)
		if(memcmp(sFormat, MQFMT_MD_EXTENSION, MQ_FORMAT_LENGTH) == 0){

			MQMDE* pMQMDE = (MQMDE*) sData;
			iDataLength   -= sizeof(MQMDE);
			sData         += sizeof(MQMDE);
			sFormat       = pMQMDE->Format;

		}else if(memcmp(sFormat, MQFMT_RF_HEADER_2, MQ_FORMAT_LENGTH) == 0){

			MQRFH* pMQRFH  = (MQRFH*) sData;
			iDataLength   -= pMQRFH->StrucLength;
			sData         += pMQRFH->StrucLength;
			sFormat       =  pMQRFH->Format;
		}else
			break;


	fprintf(fOut,"%.*s---- END OF MESSAGE -----\n",iDataLength,sData);
}
/**********************************************************************************/
static void printQLoad(FILE* fOut, Buffer *pBuffer){


	PMQCHAR  sData       = pBuffer->pData;
	int      iDataLength = pBuffer->header.iDataLength;

	char     sTimeBuffer [IAE_TIME_LEN];

	MQXQH* pMQXQH = (MQXQH*) sData;
	MQMD1* pMQMD = (MQMD1*) &(pMQXQH->MsgDesc);

	iDataLength  -= sizeof(MQXQH);
	sData        += sizeof(MQXQH);


	int bHasMDE = memcmp(pMQMD->Format, MQFMT_MD_EXTENSION, MQ_FORMAT_LENGTH) == 0;

	strftime(sTimeBuffer,IAE_TIME_LEN,"%F-%T",	gmtime(&(pBuffer->header.iTimestamp)));

	fprintf(fOut,"* QLOAD Version: 1.9 format generated Invenire Aude Exit Data Extractor\n");
	fprintf(fOut,"* Transmitted  :  %s\n", sTimeBuffer );
	fprintf(fOut,"* Remote QMgr  :  %.*s\n", MQ_Q_NAME_LENGTH, pMQXQH->RemoteQMgrName);
	fprintf(fOut,"* Remote Queue :  %.*s\n", MQ_Q_MGR_NAME_LENGTH, pMQXQH->RemoteQName);
	fprintf(fOut,"\n");

	fprintf(fOut,"A VER %d\n",pMQMD->Version + 1);
	fprintf(fOut,"A RPT %d\n",pMQMD->Report);
	fprintf(fOut,"A MST %d\n",pMQMD->MsgType);
	fprintf(fOut,"A EXP %d\n",pMQMD->Expiry);
	fprintf(fOut,"A FDB %d\n",pMQMD->Feedback);
	fprintf(fOut,"A ENC %d\n",pMQMD->Encoding);
	fprintf(fOut,"A CCS %d\n",pMQMD->CodedCharSetId);
	fprintf(fOut,"A FMT %.*s\n",MQ_FORMAT_LENGTH,pMQMD->Format);
	fprintf(fOut,"A PRI %d\n",pMQMD->Priority);
	fprintf(fOut,"A PER %d\n",pMQMD->Persistence);

	printHex(fOut,"A MSI %s\n", (void*)(pMQMD->MsgId),    MQ_MSG_ID_LENGTH);
	printHex(fOut,"A COI %s\n", (void*)(pMQMD->CorrelId), MQ_CORREL_ID_LENGTH);

	fprintf(fOut,"A BOC %d\n",pMQMD->BackoutCount);

	fprintf(fOut,"A RTQ %.*s\n",MQ_Q_NAME_LENGTH, pMQMD->ReplyToQ);
	fprintf(fOut,"A RTM %.*s\n",MQ_Q_MGR_NAME_LENGTH, pMQMD->ReplyToQMgr);

	fprintf(fOut,"A USR %.*s\n",MQ_USER_ID_LENGTH, pMQMD->UserIdentifier);

	printHex(fOut,"A ACC %s\n", (void*)(pMQMD->AccountingToken),  MQ_ACCOUNTING_TOKEN_LENGTH);
	printHex(fOut,"A AIX %s\n", (void*)(pMQMD->ApplIdentityData), MQ_APPL_IDENTITY_DATA_LENGTH);

	fprintf(fOut,"A PAT %d\n",pMQMD->PutApplType);
	fprintf(fOut,"A PAN %.*s\n",MQ_APPL_NAME_LENGTH, pMQMD->PutApplName);

	fprintf(fOut,"A PTD %.*s\n",MQ_PUT_DATE_LENGTH, pMQMD->PutDate);
	fprintf(fOut,"A PTT %.*s\n",MQ_PUT_TIME_LENGTH, pMQMD->PutTime);

	printHex(fOut,"A AOX %s\n", (void*)(pMQMD->ApplOriginData), MQ_APPL_ORIGIN_DATA_LENGTH);

	if(bHasMDE){

		MQMDE* pMQME   = (MQMDE*)(sData);
		iDataLength   -= sizeof(MQMDE);
		sData         += sizeof(MQMDE);

		printHex(fOut,"A GRP %s\n", (void*)(pMQME->GroupId), MQ_GROUP_ID_LENGTH);
		fprintf(fOut,"A MSQ %d\n",pMQME->MsgSeqNumber);
		fprintf(fOut,"A OFF %d\n",pMQME->Offset);
		fprintf(fOut,"A MSF %d\n",pMQME->MsgFlags);
		fprintf(fOut,"A ORL %d\n",pMQME->OriginalLength);

	}else{

		static MQMD DftMQMD = { MQMD_DEFAULT };

		printHex(fOut,"A GRP %s\n", (void*)(DftMQMD.GroupId), MQ_GROUP_ID_LENGTH);
		fprintf(fOut,"A MSQ %d\n",DftMQMD.MsgSeqNumber);
		fprintf(fOut,"A OFF %d\n",DftMQMD.Offset);
		fprintf(fOut,"A MSF %d\n",DftMQMD.MsgFlags);
		fprintf(fOut,"A ORL %d\n",DftMQMD.OriginalLength);

	}

	while(iDataLength > 0){

		int iChunkSize = iDataLength;

		if(iChunkSize > 25)
			iChunkSize = 25;

		printHex(fOut,"X %s\n", (void*)sData, iChunkSize);
		sData       += iChunkSize;
		iDataLength -= iChunkSize;
	}

	fprintf(fOut,"\n");
}
/**********************************************************************************/
int readMessage(int iIn, FILE *fOut){

	int rc = 0;

	rc = readBytes(iIn, (char*) &TheBuffer.header, sizeof(IAE_CHFL_Header));

	if(rc != 1)
		goto out;

	if(TheBuffer.header.iDataLength > TheBuffer.iBufferLen){

		if(TheBuffer.pData)
			free(TheBuffer.pData);

		TheBuffer.pData = malloc(TheBuffer.header.iDataLength+1);
		TheBuffer.iBufferLen = TheBuffer.header.iDataLength+1;

	}

	rc = readBytes(iIn, TheBuffer.pData, TheBuffer.header.iDataLength);
	TheBuffer.pData[TheBuffer.header.iDataLength]=0;

	if(rc != 1)
		goto out;

	rc = 1;

	if(fOut){

		MQXQH* pMQXQH = (MQXQH*) TheBuffer.pData;

		int iOffset = sizeof(MQXQH)  +
				memcmp(pMQXQH->MsgDesc.Format, MQFMT_MD_EXTENSION, MQ_FORMAT_LENGTH) == 0 ? sizeof(MQMDE) : 0;

		if(*TheOptions.sQMgrName &&
			strncmp(TheOptions.sQMgrName, pMQXQH->RemoteQMgrName, strlen(TheOptions.sQMgrName)) != 0)
			goto out;

		if(*TheOptions.sQueueName &&
				strncmp(TheOptions.sQueueName, pMQXQH->RemoteQName, strlen(TheOptions.sQueueName)) != 0)
			goto out;

		if(*TheOptions.sDataSearch &&
			strstr(TheBuffer.pData + iOffset, TheOptions.sDataSearch) == 0)
			goto out;

		if(*TheOptions.sQMgrPattern &&
				matchMQString(&TheOptions.reQMgr,pMQXQH->RemoteQMgrName,MQ_Q_MGR_NAME_LENGTH) != 1)
			goto out;

		if(*TheOptions.sQueuePattern &&
				matchMQString(&TheOptions.reQueue,pMQXQH->RemoteQName,MQ_Q_NAME_LENGTH) != 1)
			goto out;

		if(*TheOptions.sDataPattern &&
				matchMQString(&TheOptions.reData,TheBuffer.pData + iOffset, TheBuffer.header.iDataLength - iOffset) != 1)
			goto out;


		switch (TheOptions.iDataFormat) {
		case DF_QLOAD:
			printQLoad(fOut, &TheBuffer);
			break;

		case DF_DATA:
			printData(fOut, &TheBuffer);
			break;
		default:
			break;
		}

	}


out:
	return rc;
}
/**********************************************************************************/
int main(int argc, char *argv[]){

	int   iIn = -1;
	FILE  *fOut = NULL;

	readOptions(argc,argv);

	iIn  = openInputFile();

	if(!(fOut = openOutputStream()))
		goto out;

	while(TheOptions.iOffset-- > 0)
		if(readMessage(iIn, NULL) != 1)
			goto out;

	while(TheOptions.iNumMsgs-- > 0)
		if(readMessage(iIn, fOut) != 1)
			goto out;

out:

	if(TheBuffer.pData)
		free(TheBuffer.pData);

	closeInputFile(iIn);
	closeOutputStream(fOut);

	return 0;
}
/**********************************************************************************/
