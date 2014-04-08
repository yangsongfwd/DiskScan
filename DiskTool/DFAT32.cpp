/***********************************************************************
 * FileName:	DFAT32.cpp
 * Author:		����
 * Created:		2012��3��20�� ���ڶ�
 * Purpose:		DFat32��ʵ��
 * Comment:		�������Ҫ�ǰ�����һЩFAT32�Ĳٷ����⣬���ڳ���һ��FAT32�ľ�
 * 
 * Modified:	2012��5��13�� ������		�޸�һ��BUG��
 *				1.����������ļ����պ���8.3��ʽʱ���޷�ʶ������⡣GetSegName
 *				�У��ļ�������չ���ָ���"."������������ʹ8��
 *
 *				2.windows��Դ��������֧�ִ�����"."��ͷ���ļ�������ʵ����
 *				��windows���Ǵ����������ļ���,�������ڽ�ԭ�Ȳ�֧���Ե㿪
 *				ͷ���ļ�����Ϊ��֧��"."��ͷ���ļ�
 ***********************************************************************/

#include "disktool.h"
#include "stut_connf.h"


DFat32::DFat32(void)
{
	this->mDev				= INVALID_HANDLE_VALUE;
	this->mDevName[0]		= 0;
	this->mFSOff.QuadPart	= 0;
/*	this->mIsInit			= FALSE;*/
	this->mSecPerClus		= 0;
	this->mResSec			= 0;
	this->mFATs				= 0;
	this->mSecsPerFAT		= 0;
	this->mSectors			= 0;
	this->m1stDirClut		= 0;
	this->mFSinfoSec		= 0;
	this->mIsViewChged		= 0;
	this->mViewSec			= 0;
	this->mMaxClust			= 0;
	memset(this->mView , 0 , SECTOR_SIZE);
}

DFat32::~DFat32(void)
{
}

DRES DFat32::OpenDev(const char* name, LONG_INT offset)
{
	DRES		res = DR_OK;
	char		buf[SECTOR_SIZE] = {0};			//���ݻ���
	PFAT32_DBR	pDBR = PFAT32_DBR(buf);
	
	if (strlen(this->mDevName))	return DR_ALREADY_OPENDED;				//�Ѿ���ʼ��
	if (!strlen(name))	return DR_INVALED_PARAM;	//��������
	::strcpy(this->mDevName , name);
	this->mFSOff = offset;
	
	//���豸
	this->mDev = ::CreateFileA( this->mDevName , GENERIC_READ | GENERIC_WRITE,		 //����ģʽ
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL ,OPEN_EXISTING, 0 ,NULL);
	if (mDev == INVALID_HANDLE_VALUE)				
	{//���豸ʧ��
		strcpy(mDevName , "");
		return DR_OPEN_DEV_ERR;
	}
	
	this->mSectors = 1;  //Ϊ�˶�ȡ��һ��������ʱ����Ϊ1
	res = ReadData(buf , 0 ,SECTOR_SIZE );	//��ȡ�����ĵ�һ������������
	this->mSectors = 0;  //��ԭ����
	if(res)	return res;						//��ȡ����ʧ��
	if(pDBR->bsEndSig != MBR_END)//������Ч
		return DR_INIT_ERR;
	
	this->mSecPerClus = pDBR->bsSecPerClus;	//ÿ��������
	this->m1stDirClut = pDBR->bsFirstDirEntry32;//��һ����Ŀ¼���ڵĴغ�
	this->mFATs		  = pDBR->bsFATs;		//fat����
	this->mFSinfoSec  = pDBR->bsFsInfo32;	//FSinfo���ڵ�����
	this->mResSec	  = pDBR->bsResSectors; //����������	
	this->mSecsPerFAT = pDBR->bsFATsecs32;	//ÿ��FAT����ռ��������
	this->mSectors	  = pDBR->bsHugeSectors;//������������
	this->mMaxClust   = (mSectors - (mResSec + (mFATs * mSecsPerFAT))) / mSecPerClus + 2 -1;

	//�豸��ʧ��
	if(res)	strcpy(mDevName , "");

	return res;
}

void DFat32::CloseDev()
{
	strcpy(mDevName , "");
	CloseHandle(mDev);
	this->mDev				= INVALID_HANDLE_VALUE;
	this->mFSOff.QuadPart	= 0;
	this->mSecPerClus		= 0;
	this->mResSec			= 0;
	this->mFATs				= 0;
	this->mSecsPerFAT		= 0;
	this->mSectors			= 0;
	this->m1stDirClut		= 0;
	this->mFSinfoSec		= 0;
	this->mIsViewChged		= 0;
	this->mViewSec			= 0;
	this->mMaxClust			= 0;
	memset(this->mView , 0 , SECTOR_SIZE);
}


BOOL DFat32::IsDevOpened()
{
	return strlen(this->mDevName) != 0;
}

DRES DFat32::ReadData(void* buf , DWORD off , DWORD dwReadCnt)
{
	DRES	 res = DR_OK;
	DWORD	 dwReaded = 0;
	LONG_INT offset;

	//�豸��û��
	if (0 == strlen(this->mDevName)) return DR_NO_OPEN;

	//��Ҫ�ж϶�ȡ����ʱ�Ƿ�Խ��
	if (off  >= this->mSectors )
	{//��ȡ����Խ����
		return DR_DEV_CTRL_ERR;
	}
	
	offset.QuadPart = this->mFSOff.QuadPart + off;						//��ȡ���ݵ�ʵ��ƫ��
	offset.QuadPart *= SECTOR_SIZE;					//�ֽ�ƫ��



	//�����ļ�ָ��
	offset.LowPart = SetFilePointer(this->mDev , offset.LowPart , PLONG(&(offset.HighPart)) ,FILE_BEGIN );
	if (offset.LowPart == -1 && GetLastError() != NO_ERROR )
		res = DR_DEV_CTRL_ERR;

	//��ȡ����
	if(!res && !::ReadFile(this->mDev , buf , dwReadCnt ,&dwReaded ,NULL) && dwReaded != dwReadCnt)	
		res =  DR_DEV_IO_ERR;
// 
// 	CloseDevice(hDev);								//�ر��Ѿ��򿪵��豸
	return res;
}

DRES DFat32::WriteData(void* buf , DWORD off , DWORD dwWrite)
{
	DRES	 res = DR_OK;
	DWORD	 dwWrited = 0;
	LONG_INT offset = {0};

	offset.QuadPart = this->mFSOff.QuadPart + off;	//��ȡ���ݵ�ʵ��ƫ��
	offset.QuadPart *= SECTOR_SIZE;					//�ֽ�ƫ��

	//�����ļ�ָ��
	offset.LowPart = SetFilePointer(this->mDev , offset.LowPart , PLONG(&(offset.HighPart)) ,FILE_BEGIN );
	if (offset.LowPart == -1 && GetLastError() != NO_ERROR )
		res = DR_DEV_CTRL_ERR;

	//��ȡ����
	if(!res && !WriteFile(this->mDev , buf , dwWrite , &dwWrited ,NULL) && dwWrited != dwWrite)	
		res =  DR_DEV_IO_ERR;

/*	CloseDevice(hDev);								//�ر��Ѿ��򿪵��豸*/
	return res;
}
DWORD DFat32::GetFATFromFAT1(DWORD clust)
{
	DWORD fsect;

	if (clust < 2 || clust > this->mMaxClust)	//�غż��
		return 1;
	fsect = this->mResSec;
	if (MoveView(fsect + (clust / (SECTOR_SIZE / 4))))//�ƶ����ڵ�clust���ڵ�����
		0xFFFFFFFF;

	return GetDWORD(mView + (clust % (SECTOR_SIZE / 4) *4)) & FAT_MASK;
}
DWORD DFat32::GetFATFromFAT2(DWORD clust)
{
	DWORD fsect;

	if (clust < 2 || clust > this->mMaxClust)	//�غż��
		return 1;
	fsect = this->mResSec + this->mSecsPerFAT;
	if (MoveView(fsect + (clust / (SECTOR_SIZE / 4))))//�ƶ����ڵ�clust���ڵ�����
		0xFFFFFFFF;

	return GetDWORD(mView + (clust % (SECTOR_SIZE / 4) *4)) & FAT_MASK;
}

DWORD DFat32::ClustToSect(DWORD clust)
{
	if (0 == strlen(mDevName)) return 0;
	if (clust < 2 || clust > this->mMaxClust) 
		return 0;				//��Ч�Ĵغ�
	return ((clust - 2) * this->mSecPerClus) + this->mResSec + (this->mFATs * this->mSecsPerFAT);
}

DWORD DFat32::SectToClust(DWORD sector)
{
	if (0 == strlen(mDevName)) 
		return 0;

	if (sector < (this->mResSec + (this->mFATs * this->mSecsPerFAT)))
	{//��Ч������
		return 0;
	}

	//��Ը�Ŀ¼�ĵ�һ��
	sector -= (this->mResSec + (this->mFATs * this->mSecsPerFAT));

	//���ʵ�ʵĴغ�
	return (sector/ this->mSecPerClus) + 2;
}

DRES DFat32::MoveView(DWORD sec)
{
	DWORD	vSect = this->mViewSec;
	DRES	res = DR_OK;

	if (vSect != sec) {					//Ҫ�ı䵱ǰ�����ݴ���
		if (this->mIsViewChged) {		//������ͼ�е�������Ҫд�����
			if ((res = WriteData(this->mView, vSect ,SECTOR_SIZE)) != DR_OK)
				return res;
			this->mIsViewChged = FALSE;
			if (vSect < (this->mResSec + this->mSecsPerFAT)) {	//��FAT������
				int nf;
				for (nf = this->mFATs; nf > 1; nf--) {			//������д��ÿһ��FAT����ȥ
					vSect += this->mSecsPerFAT;
					WriteData(this->mView, vSect  ,SECTOR_SIZE);
				}
			}
		}
		if (sec) {
			if ((res = ReadData(this->mView, sec  , SECTOR_SIZE)) != DR_OK)
				return res;
			this->mViewSec = sec;
		}
	}
	return DR_OK;
}
DRES DFat32::PosEntry(PVOID entr , WORD index)
{
	DWORD	  clst;
	USHORT	  ic;
	PDirEntry entry = PDirEntry(entr);

	entry->mIndex = index;						//Ҫ��λ��Ŀ¼��
	clst = entry->mStartClust;					//��ʼ�غ�
	if (clst == 1 || clst > this->mMaxClust)	//�����ʼ�غ��Ƿ���Ϸ�
		return DR_INIT_ERR;

	ic = SECTOR_SIZE / 32 * this->mSecPerClus;	//ÿ�ص������
	while (index >= ic) {						//����������
		clst = GetFATFromFAT1( clst	);					//�����һ�صĴغ�
		if (clst == 0xFFFFFFFF) 
			return DR_DEV_IO_ERR;				//�豸IO����
		if (clst < 2 || clst > this->mMaxClust)//�����˷�Χ
			return DR_INIT_ERR;
		index -= ic;
	}

	entry->mCurClust = clst;						//��ǰ�غ�
	entry->mCurSect = ClustToSect( clst) + index / (SECTOR_SIZE / 32);	//������
	entry->mDir = this->mView + (index % (SECTOR_SIZE / 32)) * 32;//��λindex��ָ�������
	return DR_OK;								//��λ�ɹ�  ֻ�����õ�ʱ���ƶ����ݴ�����
}

DRES DFat32::GetSegName(const WCHAR** path, PVOID entr)
{
	PDirEntry	entry	= PDirEntry(entr);
	WCHAR*		lfn		= entry->mLFN;
	char*		sfn		= (char*)entry->mSFN;
/*	const WCHAR* p		= *path;*/
	const WCHAR* p		= NULL;
	WCHAR		w		= 0;	//Unicode�ַ�����
	int			i		= 0;	//��������
	int			lfni	= 0;	//lfn��β
	int			sfni	= 0;	//sfn��β
	int			doti	= 0;	//���һ�����λ��
	W_CHAR		w_w		= {0};		//�����ַ�ת�����ַ�
	
	if (path[0][0] == '*')
	{//�Ѿ�ɾ���˵��ļ�
		p = *path + 1;
		entry->mIsDelFile =  TRUE;
	}else{
		entry->mIsDelFile =  FALSE;
		p = *path;
	}
	entry->mStatus = 0;				//������һ��״̬
	::memset(entry->mLFN , 0 ,sizeof(WCHAR)*MAX_LFN);
	::memset(entry->mSFN , 0 ,11);

	for (i = 0; !IsPathSeparator(w = p[i]) && w  ; ++i)
	{
		//���ļ����пո�͵�������м䡣���������ڿ�ʼ��ĩβ
		//�ļ���ǰ�治�����пո���ߵ�
		if(!lfni && (w <= 0x20 /*|| w == '.'*/ ))	//windows��Դ��������Ȼ���ɴ����Ե㿪ͷ���ļ���������ȴ����ȴ�Ǵ��ڵ�  2012-5-13
			continue;
		if(lfni > MAX_LFN) return DR_INVALID_NAME;	//�ļ���������255���ַ�
		if(IsSingleByteChar(w) && strchr("\"*:<>\?|", w))	return DR_INVALID_NAME;					//�зǷ��ַ�

		lfn[lfni++] = w;
	}
	if(!w)	//�������һ��
		entry->mStatus |= ST_LAST;
	*path = p + i +1;//����ʣ�µ�·��
	
	lfn[lfni] = 0;	
	//ȥ������Ŀո�͵�
	for (; lfni > 0 &&(lfn[lfni-1]== 0x20 || lfn[lfni-1]== '.');--lfni)
		lfn[lfni-1] = 0;
	if(!lfni)  return DR_INVALID_NAME;//û������

	//ͬʱ�д�д��д��ʱ��Ҳֻ���ó��ļ���
	if(entry->mStatus & ST_UPPER && entry->mStatus & ST_LOWER)
		entry->mStatus |= ST_LFN;
	
	//���ļ����������
	//��ʼ���ļ���

	//������չ����λ��  //��һ���ַ��������ǵ�
	for (doti = lfni-1; doti >= 0 && lfn[doti] != '.';--doti);  //windows ��ʵ�ǿ��Դ���һ�㿪ͷ���ļ���     
	//doti==-1  û��չ��
	if(doti == -1) 
		doti = 0;
	else if(entry->mIsDelFile == FALSE && doti >/*=*/ 8)	//������8.3������   //�˴������е��� "."�����λ���ǿ��Ե���8��  2012-5-13
		entry->mStatus |= ST_LFN;
	else if (entry->mIsDelFile && doti >/*=*/ 7)
	{
		entry->mStatus |= ST_LFN;
	}

	//��ʼ�������ļ���
	//����С��0x20 �Լ�"\"*+,./:;<=>\?[\\]|"
	//�����ļ�������

	sfni = 0;
	if (entry->mIsDelFile && !(entry->mStatus & ST_LFN)){  //�Ѿ�ɾ���˵��ļ��Ķ��ļ�����һ���ֽ��滻�� '*'
		sfn[sfni++] = '*';

	}

	for (i = 0 ; i < lfni; ++i )
	{
		if(i == doti && doti)  break;  //����չ����
		w = lfn[i];
		if (IsSingleByteChar(w) && strchr(" .+,/;=[\\]", w))  //�������ļ����в����Ե� ����" +,/;=[\\]"  ��һ���ո�
		{//�����˷Ƿ��ַ�  ֻ���ó��ļ���
			entry->mStatus |= ST_LFN;
			continue;
		}
		
		//���ļ���������д�д����Сд��ֻ���ó��ļ���
		if(!(entry->mStatus & ST_LFN))
		{
			if (IsCharUpper((char)w))  entry->mStatus |= ST_UPPER;
			if (IsCharLower((char)w))  entry->mStatus |= ST_LOWER;
		}

		w_w.charw = WchrToUpper(w);		//��Ҫ�Ļ��Ƚ��ַ�ת���ɴ�С����ʽ
		w_w = ChrConvert(w_w , FALSE);	//�ڽ��ַ�ת���ɶ��ֽ��ַ�
		if(w_w.char1){
			sfn[sfni++] = w_w.char1;
			if(sfni == 8) break;
		}
		sfn[sfni++] = w_w.char2;
		if(sfni == 8) break;
	}

	//������еĿո�
	for (;sfni < 8;++sfni)	sfn[sfni] = 0x20;

	//����չ����
	if(doti){   //����չ��
		for (i = doti +1 ; i < lfni ; ++i)
		{
			w = lfn[i];
			if (IsSingleByteChar(w) && strchr(" +,/;=[\\]", w))  //�������ļ����в����Ե� ����" +,/;=[\\]"  ��һ���ո�
			{//�����˷Ƿ��ַ�  ֻ���ó��ļ���
				entry->mStatus |= ST_LFN;
				continue;
			}
			w_w.charw = WchrToUpper(w);		//��Ҫ�Ļ��Ƚ��ַ�ת���ɴ�С����ʽ
			w_w = ChrConvert(w_w , FALSE);	//�ڽ��ַ�ת���ɶ��ֽ��ַ�
			if(w_w.char1){
				sfn[sfni++] = w_w.char1;
				if(sfni == 11) break;
			}
			sfn[sfni++] = w_w.char2;
			if(sfni == 11) break;
		}
	}
	//��չ�������Ļ���ո�
	for (;sfni < 11;++sfni)	sfn[sfni] = 0x20;

	return DR_OK;
}
// DRES DFat32::GetDelSegName(const WCHAR** path, PVOID entr)
// {
// 	PDirEntry	entry	= PDirEntry(entr);
// 	WCHAR*		lfn		= entry->mLFN;
// 	char*		sfn		= (char*)entry->mSFN;
// 	const WCHAR* p		= (*path) + 1;   //������һ���ֽ�  "*"
// 	WCHAR		w		= 0;	//Unicode�ַ�����
// 	int			i		= 0;	//��������
// 	int			lfni	= 0;	//lfn��β
// 	int			sfni	= 0;	//sfn��β
// 	int			doti	= 0;	//���һ�����λ��
// 	W_CHAR		w_w		= {0};	//�����ַ�ת�����ַ�
// 
// 	entry->mStatus = 0;			//������һ��״̬
// 	::memset(entry->mLFN , 0 ,sizeof(WCHAR)*MAX_LFN);
// 	::memset(entry->mSFN , 0 ,11);
// 
// 	for (i = 0; !IsPathSeparator(w = p[i]) && w  ; ++i)
// 	{
// 		//���ļ����пո�͵�������м䡣���������ڿ�ʼ��ĩβ
// 		//�ļ���ǰ�治�����пո���ߵ�
// 		if(!lfni && (w <= 0x20 /*|| w == '.'*/ ))	//windows��Դ��������Ȼ���ɴ����Ե㿪ͷ���ļ���������ȴ����ȴ�Ǵ��ڵ�  2012-5-13
// 			continue;
// 		if(lfni > MAX_LFN) return DR_INVALID_NAME;	//�ļ���������255���ַ�
// 		if(IsSingleByteChar(w) && strchr("\"*:<>\?|", w))	return DR_INVALID_NAME;					//�зǷ��ַ�
// 
// 		lfn[lfni++] = w;
// 	}
// 	if(!w)	//�������һ��
// 		entry->mStatus |= ST_LAST;
// 	*path = p + i +1;//����ʣ�µ�·��
// 
// 	lfn[lfni] = 0;	
// 	//ȥ������Ŀո�͵�
// 	for (; lfni > 0 &&(lfn[lfni-1]== 0x20 || lfn[lfni-1]== '.');--lfni)
// 		lfn[lfni-1] = 0;
// 	if(!lfni)  return DR_INVALID_NAME;//û������
// 
// 	//ͬʱ�д�д��д��ʱ��Ҳֻ���ó��ļ���
// 	if(entry->mStatus & ST_UPPER && entry->mStatus & ST_LOWER)
// 		entry->mStatus |= ST_LFN;
// 
// 	//���ļ����������
// 	//��ʼ���ļ���
// 
// 	//������չ����λ��  //��һ���ַ��������ǵ�
// 	for (doti = lfni-1; doti >= 0 && lfn[doti] != '.';--doti);  //windows ��ʵ�ǿ��Դ���һ�㿪ͷ���ļ���     
// 	//doti==-1  û��չ��
// 	if(doti == -1) 
// 		doti = 0;
// 	else if(doti >/*=*/ 8)	//������8.3������   //�˴������е��� "."�����λ���ǿ��Ե���8��  2012-5-13
// 		entry->mStatus |= ST_LFN;
// 
// 	//��ʼ�������ļ���
// 	//����С��0x20 �Լ�"\"*+,./:;<=>\?[\\]|"
// 	//�����ļ�������
// 	for (sfni = 0 ,i = 0 ; i < lfni; ++i )
// 	{
// 		if(i == doti && doti)  break;  //����չ����
// 		w = lfn[i];
// 		if (IsSingleByteChar(w) && strchr(" .+,/;=[\\]", w))  //�������ļ����в����Ե� ����" +,/;=[\\]"  ��һ���ո�
// 		{//�����˷Ƿ��ַ�  ֻ���ó��ļ���
// 			entry->mStatus |= ST_LFN;
// 			continue;
// 		}
// 
// 		//���ļ���������д�д����Сд��ֻ���ó��ļ���
// 		if(!(entry->mStatus & ST_LFN))
// 		{
// 			if (IsCharUpper((char)w))  entry->mStatus |= ST_UPPER;
// 			if (IsCharLower((char)w))  entry->mStatus |= ST_LOWER;
// 		}
// 
// 		w_w.charw = WchrToUpper(w);		//��Ҫ�Ļ��Ƚ��ַ�ת���ɴ�С����ʽ
// 		w_w = ChrConvert(w_w , FALSE);	//�ڽ��ַ�ת���ɶ��ֽ��ַ�
// 		if(w_w.char1){
// 			sfn[sfni++] = w_w.char1;
// 			if(sfni == 8) break;
// 		}
// 		sfn[sfni++] = w_w.char2;
// 		if(sfni == 8) break;
// 	}
// 
// 	//������еĿո�
// 	for (;sfni < 8;++sfni)	sfn[sfni] = 0x20;
// 
// 	//����չ����
// 	if(doti){   //����չ��
// 		for (i = doti +1 ; i < lfni ; ++i)
// 		{
// 			w = lfn[i];
// 			if (IsSingleByteChar(w) && strchr(" +,/;=[\\]", w))  //�������ļ����в����Ե� ����" +,/;=[\\]"  ��һ���ո�
// 			{//�����˷Ƿ��ַ�  ֻ���ó��ļ���
// 				entry->mStatus |= ST_LFN;
// 				continue;
// 			}
// 			w_w.charw = WchrToUpper(w);		//��Ҫ�Ļ��Ƚ��ַ�ת���ɴ�С����ʽ
// 			w_w = ChrConvert(w_w , FALSE);	//�ڽ��ַ�ת���ɶ��ֽ��ַ�
// 			if(w_w.char1){
// 				sfn[sfni++] = w_w.char1;
// 				if(sfni == 11) break;
// 			}
// 			sfn[sfni++] = w_w.char2;
// 			if(sfni == 11) break;
// 		}
// 	}
// 	//��չ�������Ļ���ո�
// 	for (;sfni < 11;++sfni)	sfn[sfni] = 0x20;
// 
// 	return DR_OK;
// }

DRES DFat32::GetDirEntry( const WCHAR* path ,PVOID entr)
{
	DRES		res  = DR_OK;
	BYTE*		dir  = NULL; 
	BYTE		attr = 0;
	PDirEntry	entry= PDirEntry(entr);

	if (IsPathSeparator(path[0]))	++path;	//����ǰ��ķָ���
	entry->mStartClust = this->m1stDirClut;	//�Ӹ�Ŀ¼��ʼ
	if(path[0] == 0){						//��·��Ҳ��һλ���ǵ�ǰ��������
		res = PosEntry(entry , 0);			//��λ���ƶ���������ڵ�ɽ��
		entry->mDir = 0;					//��Ϊû��ֻ���κ�Ŀ¼ ���ǵ�ǰ���� ����û�����
	}else{									//һ��ָ����Ŀ¼
		while (TRUE)
		{
			res = GetSegName(&path ,entry);	//���path�еĵ�һ��·����
		
			if(res)  break;					//�ļ���������
			
			if (entry->mIsDelFile)
			{//�����Ѿ�ɾ�����˵����
				res = FindDelEntry(entry);
			}else{
				//�������
				res = FindEntry(entry);			//��ָ��������/���в����ƶ������
			}

			
			if(res != DR_OK)
			{//����ʧ��
				if(res == DR_FAT_EOF)   //���������Ѿ�����  
					res = DR_NO_PATH;	//û��ָ����·��
				break;
			}
			//�˴γɹ�
			if(entry->mStatus & ST_LAST)	//���һ���Ѿ�ƥ���� �ҵ��� 
				break;

			dir = entry->mDir;
			attr = PSDE(dir)->mAttr;
			if(!(attr & ATTR_DIRECTORY)){	//����һ��Ŀ¼ �޷��ٸ���ȥ��
				res = DR_NO_PATH;
				break;
			}
			entry->mStartClust = (PSDE(dir)->mFstClusHI << 16)|PSDE(dir)->mFstClusLO;
		}
	}
	return res;
}

DRES DFat32::FindEntry(PVOID entr)
{
	DRES	res		= DR_OK;
	BYTE*	dir		= NULL;
	BYTE	flag	= 0;
	BYTE	attr	= 0;
	BYTE	chSum	= 0;		//���ļ���У���
	BYTE	order	= 0;		//Ŀ¼�����
	PDirEntry entry = PDirEntry(entr);

	res = PosEntry(entry , 0);		//�ض�λ����һ����ڵ�λ�� ��λ������0Ŀ¼��λ��
	if (res) return res;			//�Ӷ�λʧ��
	
	do{	//����Ŀ¼��ÿһ�����
		res = MoveView(entry->mCurSect);
		if(res)  break;
		dir = entry->mDir;			//��ڵ�λ��
		flag = dir[0];
		if( flag == 0){				//����Ŀ¼��ĩβ
			res = DR_NO_FILE;
			break;		
		}
		attr = PSDE(dir)->mAttr & ATTR_FAT32_MASK;//Ŀ¼�������
		
		if (flag == 0xE5 || \
			((attr & ATTR_VOLUME_ID) && attr != ATTR_LONG_NAME)) {	//����һ����Ч�����
			order = 0xFF;
		} else {//��һ����Ч�����
			if(attr == ATTR_LONG_NAME){//���ļ������
				if(flag & 0x40)	{	//��һ�����ļ������еĿ�ʼ  Ҳ����һ�����ļ��������һ������
					chSum = PLDE(dir)->mChksum;//�ļ�����У���
					flag &= 0xBF;	//ȥ��0x40������  �ٳ����
					order = flag;			
				}
				//������һ������Ŀ¼�����
				order = (order == flag && chSum == PLDE(dir)->mChksum &&CompLFN(entry->mLFN ,dir))? order - 1 : 0xFF;

			}else{  //һ�����ļ���
				if(!order && chSum == ChkSum(dir))
					break;			//�ҵ���ƥ��ĳ�������
				order = 0xFF;
				if(!(entry->mStatus & ST_LFN)&& CompSFN((char*)entry->mSFN , (char*)dir)) break;
			}
		}
		res = NextEntry(entry);  //�Ƶ�дһ�����
	} while (!res);

	return res;
}

DRES DFat32::FindDelEntry(PVOID entr)
{
	DRES		res		= DR_OK;
	BYTE*		dir		= NULL;
	BYTE		flag	= 0;
	BYTE		attr	= 0;
	USHORT		chSum	= 0xFFFF;	//�����У���Ϊ�����ֽ�
									//��Ҫ����Ϊ�ô������ж��Ƿ���һ���µĳ��ļ����Ŀ�ʼ
									//ӦΪʵ�ʵ�У�����һ���ֽ�,Ҳ����˵����Ϊ0xFF 
									//������0xFFFF����ǰ��û��ƥ�䵽һ������Ŀ¼��
	BYTE		order	= 0;		//Ŀ¼�����
	PDirEntry	entry = PDirEntry(entr);
	WCHAR		nambuf[MAX_LFN+1] = {0}; //�ļ�������
	WCHAR       path[MAX_PATH] = {0};	//·������
	W_CHAR		w_w;					//���ڼ���У���

	res = PosEntry(entry , 0);		//�ض�λ����һ����ڵ�λ�� ��λ������0Ŀ¼��λ��
	if (res) return res;			//�Ӷ�λʧ��

	do{	//����Ŀ¼��ÿһ�����
		res = MoveView(entry->mCurSect);
		if(res)  break;				//����
		dir = entry->mDir;	//��ڵ�λ��
		flag = dir[0];				//��һ���ֽڵı�־λ
		if( flag == 0){				//����Ŀ¼��ĩβ
			res = DR_FAT_EOF;
			return DR_FAT_EOF;
		}
		attr = PSDE(dir)->mAttr & ATTR_FAT32_MASK;//Ŀ¼�������

		if (flag != 0xE5) {			//����һ��ɾ���˵����
			chSum = 0xFFFF;
		} else {//��һ����Ч�����
			if(attr == ATTR_LONG_NAME){//���ļ������

				if(chSum == 0xFFFF)	{
					//��һ�����ļ������еĿ�ʼ
					//Ҳ����һ�����ļ��������һ������
					chSum = PLDE(dir)->mChksum;//��¼��У���
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���
				}
				//�ж�
				if(chSum == PLDE(dir)->mChksum)
				{//ƥ�䵽һ�����ļ���
					AppLFN(nambuf , dir);//ȡ���ļ����������ӵ������ǰ��
				}else{//ƥ��ʧ�� , ����ԭ�е�ƥ����  ������һ���µ�ƥ�����
					//��ջ��� ,������һ��ƥ��
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���

					//������һ��ƥ��
					chSum = PLDE(dir)->mChksum;
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���
					AppLFN(nambuf , dir);//ȡ���ļ����������ӵ������ǰ��
				}

			}else{  //һ�����ļ���
				//���ļ�����ڵĵ�һ���ֽ�������0xE5  �Ѿ��޷�����У�����
				//�������ȳ��ļ����л�ԭ����ԭ���ټ���
				BYTE btBack;
				w_w.charw = nambuf[0];
				w_w = ChrConvert(w_w ,FALSE );
				btBack = dir[0];
				if (w_w.char1)//��һ���ַ��Ƕ��ֽ��ַ�
					dir[0] = w_w.char1;
				else		 //��һ���ַ��ǵ��ֽ��ַ�
					dir[0] = w_w.char2;

				if(chSum == ChkSum(dir)){  
					 dir[0] = btBack;   //�Ƚ����˺�Ϳ��Ի�ԭ
					//���ļ����Ƚ�
					if (0 == memcmp(nambuf , entry->mLFN , wcslen(entry->mLFN) ))
					{//�ҵ���
						break;
					}
					chSum = 0xFFFF;
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���
			
				}else{						//����Ķ��ļ������
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���

//					(*(char*)dir) = '*';//ɾ����־
					SetBYTE(dir , '*');
					//SetSFN(nambuf ,dir);

					if(!(entry->mStatus & ST_LFN)&& CompSFN((char*)entry->mSFN , (char*)dir)) 
					{
						dir[0] = btBack; 					
						break;
					}
					dir[0] = btBack; 	
				}
			}
		}
		res = NextEntry( entry );  //�Ƶ���һ�����
		if (res == DR_FAT_EOF)    //���˴���β��
		{
			return DR_FAT_EOF;
		}
	} while (!res);

	return res;
}

BOOL DFat32::CompLFN(const WCHAR* path, BYTE* dir)
{
	//���ļ���Ŀ¼����е�ÿһ���ַ����ڵ��ֽ�λ��
	static const BYTE LfnOfs[] = {1,3,5,7,9,14,16,18,20,22,24,28,30};
	BYTE	index = 0;
	WCHAR	wp	  = 0;
	WCHAR	wd	  = 0;
	int		i	  = 0;
	
	index = dir[0] & 0xBF;			//�ٳ����
	index = (index - 1) *13;		//��Ŷ�Ӧ�ĵ�һ���ַ�  ÿ��Ŀ¼���ʮ�����ַ�����Ŵ�һ��ʼ 
	do{
		wd = (WCHAR)GetWORD(dir + LfnOfs[i]);
		if(wd)
		{		//�Ѿ���������·���Ľ�β
			wp = path[index++];
			wd = WchrToUpper(wd);
			wp = WchrToUpper(wp);
			if(wd != wp)		//ƥ��ʧ��
				return FALSE;
		}else{	//�Ѿ���������·���Ľ�β
			if(1+i == 13)  //�պ����һ���ַ���0
				break;
			wd = (WCHAR)GetWORD(dir + LfnOfs[++i]);
			if(wd != 0xFFFF)	return FALSE;
			else	return TRUE;
		}
	} while (++i < 13);				//ƥ��13��  һ���������ʮ�����ַ�

	return TRUE;
}
//���ļ���Ŀ¼����е�ÿһ���ַ����ڵ��ֽ�λ��
static const BYTE LfnOfs[] = {1,3,5,7,9,14,16,18,20,22,24,28,30};
DRES DFat32::SetLFN( WCHAR* path, BYTE* dir)
{
	BYTE	index	= 0;
	int		i		= 0;

	index = dir[0] & 0xBF;			//�ٳ����
	index = (index - 1) *13;		//��Ŷ�Ӧ�ĵ�һ���ַ�  ÿ��Ŀ¼���ʮ�����ַ�����Ŵ�һ��ʼ 
	do{
		if(!(path[index++] = (WCHAR)GetWORD(dir + LfnOfs[i])))
			break;		
	} while (++i < 13);				//ƥ��13��  һ���������ʮ�����ַ�
	
	//�ļ�����ĩβ��
	//if((i == 13) && ((dir[0]&0xBF) == 1))
	//	path[index] = 0;

	return DR_OK;
}
DRES DFat32::AppLFN( WCHAR* c, BYTE* dir)
{
	WCHAR	temp  = 0;
	int		i	  = 0;
	int		len   = 0;//ԭ�е����ݳ���
	WCHAR	buf[14] = {0};
	
	for(i = 0 ; i < 13 ; ++i)
	{
		temp = (WCHAR)GetWORD(dir + LfnOfs[i]);
		if (!temp)//��β��
			break;
		buf[i] = temp;
	}
	
	//��Ԫ�����е����ݺ��� i���ַ�
	len = wcslen(c);
	if(len) ::memmove(((BYTE*)c) + i * 2 , (BYTE*)c , len*2);
	memcpy((BYTE*)c , (BYTE*)buf , i*2);

	return DR_OK;
}
DRES DFat32::SetSFN( WCHAR* path, BYTE* dir)
{
	//���ֽ��ַ�����
	char buf[20] = {0};
	int  bufi	 = 0;
	char a		 = 0;
	int  i		 = 0;
	BYTE nameCase = PSDE(dir)->mNameCase;

	//�ٳ����ֲ���
	if (nameCase & FNAME_LOWER_CASE){
		//�ļ���ΪСд
		for(; i < 8 ; ++i){ a = dir[i]; if(a!= 0x20) buf[bufi++] = ChrToLower(a); }
	}else{//�ļ���Ϊ��д
		for(; i < 8 ; ++i) { a = dir[i]; if(a!= 0x20) buf[bufi++] = a; }
	}

	//׷��һ����
	buf[bufi++] = '.';

	//ȡ����չ��
	if (nameCase & FEXT_NAME_LOWER_CASE)
	{//��չ��Ϊ��д
		for( ; (i < 11) && ((a = dir[i]) != 0x20); ++i) buf[bufi++] = ChrToLower(a);	
	}else{
		//��չ��ΪСд
		for( ; (i < 11) && ((a = dir[i]) != 0x20); ++i) buf[bufi++] = a;
	}


	//��β
	if(buf[bufi-1] == '.')	buf[bufi-1] = 0;  //û����չ��
	else	buf[bufi] = 0;			//����չ��

	MultyByteToUnic(buf , path , MAX_LFN+1);

	return DR_OK;
}
BOOL DFat32::CompSFN(const char* path1, const char* path2)
{
	for (int i = 0 ; i < 11 ;++i)
	{
		if(ChrToUpper(path1[i]) != ChrToUpper(path2[i]))
			return FALSE;
	}
	return TRUE;
}

DRES DFat32::NextEntry(PVOID entr)
{
	PDirEntry	entry	= PDirEntry(entr); 
	WORD		i		= entry->mIndex + 1;
	DWORD		idxSec	= 0;//��һ��������ڵĵ�ǰĿ¼��������
	DWORD		clust	= 0;

	if(!(i % (SECTOR_SIZE /32)))
	{								//��Ҫ������һ��������
		++entry->mCurSect;			//������������
		idxSec = i / (SECTOR_SIZE /32);
		if(idxSec && !(idxSec % this->mSecPerClus))
		{
			clust = GetFATFromFAT1(entry->mCurClust);
			if(clust == 1)
				return DR_INIT_ERR;	//��Ч�غ�
			if(clust == 0xFFFFFFFF)
				return DR_DEV_IO_ERR;//�豸IO����
			if(clust > this->mMaxClust)
				return DR_FAT_EOF;	//���˽�β��
			
			entry->mCurClust = clust;
			entry->mCurSect = ClustToSect(clust);
		}
	}
	entry->mIndex = i;
	entry->mDir = this->mView + (i % (SECTOR_SIZE / 32)) * 32;

	return DR_OK;
}

BYTE DFat32::ChkSum(BYTE* pFcbName)
{//Դ�� fatgen103.doc
	short FcbNameLen = 0;
	BYTE  Sum		 = 0;

	for (FcbNameLen=11; FcbNameLen!=0; FcbNameLen--) 
	{
		// NOTE: The operation is an unsigned char rotate right
		Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *pFcbName++;
	}
	return (Sum);
}

DRES DFat32::OpenFileA(const char* path , DFat32File* file)
{
	int		len	  = 0;
	WCHAR * wPath = NULL;

	//��������
	if (path == NULL || file == NULL) return DR_INVALED_PARAM;
	
	len = strlen(path);
	wPath = new WCHAR[len + 1];
	MultyByteToUnic(path , wPath , len + 1);

	return OpenFileW(wPath , file);
	
}

DRES DFat32::OpenFileW(const WCHAR* path , DFat32File *file)
{
	DRES	 res = DR_OK;
	DirEntry entry;

	//��ǰ�豸û�д�
	//�豸��û��
	if (0 == strlen(this->mDevName)) return DR_NO_OPEN;

	//��������
	if (path == NULL || file == NULL) return DR_INVALED_PARAM;

	::memset(&entry , 0 , sizeof(entry));

	res = GetDirEntry(path , &entry );
	if (res){			//����ʧ��
		file->mFS = NULL;	//���ص��ļ����NULL
		return res;			
	}
	file->mFS = this;
	//��ʼ�����ļ�����
	return NewFileHandle(file , &entry , path);
}

DRES DFat32::ListFile(DFat32File* fil, FIND_FILE listFun)
{
	//DFat32File*	fil		= (DFat32File*)dirc;//�Ѿ��򿪵�Ŀ¼
	DRES		res		= DR_OK;
	DirEntry	entry	= {0};		//��ڽṹ
	BYTE*		dir		= NULL;
	BYTE		flag	= 0;		//��һ���ֽڵı�׼
	BYTE		attr	= 0;		//����
	BYTE		chSum	= 0;		//���ļ���У���
	BYTE		order	= 0;		//Ŀ¼�����
	WCHAR		nambuf[MAX_LFN+1] = {0}; 
	
	//�豸��û��
	if (0 == strlen(this->mDevName)) return DR_NO_OPEN;
	
	if(!fil || !listFun )
		return DR_INVALED_PARAM;		//��������
	if(!(fil->mAttr & ATTR_DIRECTORY))
		return DR_INVALED_PARAM;		//��Ҫ����һ��Ŀ¼������һ���ļ�


	entry.mStartClust = fil->mStartClust;//����ָ����Ŀ¼�ĵ�һ��
	res = PosEntry(&entry , 0);			//��λ��һ��ɽ���ĵ�һ����ڽṹ
	if (res) return res;				//��λʧ��

	do{	//����Ŀ¼��ÿһ�����
		res = MoveView(entry.mCurSect);
		if(res)  break;				//����
		dir = entry.mDir;			//��ڵ�λ��
		flag = dir[0];				//��һ���ֽڵı�־λ
		if( flag == 0){				//����Ŀ¼��ĩβ
			(*listFun)(NULL);		//֪ͨ�ص��߲������
			break;		
		}
			
		attr = PSDE(dir)->mAttr & ATTR_FAT32_MASK;//Ŀ¼�������

		if (flag == 0xE5||\
			((attr & ATTR_VOLUME_ID) && attr != ATTR_LONG_NAME)) {	//����һ����Ч�����
				order = 0xFF;
		} else {//��һ����Ч�����
			if(attr == ATTR_LONG_NAME){//���ļ������
				if(flag & 0x40)	{	//��һ�����ļ������еĿ�ʼ  Ҳ����һ�����ļ��������һ������
					chSum = PLDE(dir)->mChksum;//�ļ�����У���
					flag &= 0xBF;	//ȥ��0x40������  �ٳ����
					order = flag;	
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���
				}
				//������һ������Ŀ¼�����
				if(order == flag && chSum == PLDE(dir)->mChksum)
				{//ƥ�䵽һ�����ļ���
					--order; 
					SetLFN(nambuf , dir);		//ȡ���ļ���
				}else{//ƥ��ʧ��
					order = 0xFF;
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���
				}

			}else{  //һ�����ļ���
				if(!order && chSum == ChkSum(dir)){
					(*listFun)(nambuf);		//�ҵ�һ�����ļ���Ŀ¼��Ӧ�Ķ��ļ������
				}else{						//����Ķ��ļ������
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���
					order = 0xFF;
					SetSFN(nambuf ,dir);
					(*listFun)(nambuf);		//�ҵ�һ�����ļ���	
				}
			}
		}
		res = NextEntry(&entry);  //�Ƶ���һ�����
		if (res == DR_FAT_EOF)    //���˴���β��
		{
			(*listFun)(NULL);		//֪ͨ�ص��߲������
			return DR_OK;
		}
	} while (!res);

	return res;
}

DRES DFat32::FindFile( DFat32File* dir , FINDER* finder , BOOL findDel /*= FALSE*/ )
{
	DRES			 res  = DR_OK;
	PFat32FileFinder pfff = NULL;
	int				 len  = 0;

	//�豸��û��
	if (0 == strlen(this->mDevName)) return DR_NO_OPEN;

	//������һ�½��
	*finder = NULL;

	//���а�ȫ���
	if (!dir || !finder) return DR_INVALED_PARAM;
	if (!dir->IsValid()) return DR_INVALID_HANDLE;
	if (!dir->IsDir())	 return DR_IS_FILE;
	
	//����һ�����Ҿ��
	pfff = new Fat32FileFinder();
	memset(pfff , 0 , sizeof(Fat32FileFinder));
	
	//�Ƿ���Ҫ�����Ѿ�ɾ���˵��ļ�
	pfff->isFindDel = findDel;
	wcscpy(pfff->path , dir->mPath);
	pfff->isEnd = FALSE;
// 	len = wcslen(pfff->path);
// 	//���ӷָ��
// 	if (!IsPathSeparator(pfff->path[len - 1]))
// 	{
// 		pfff->path[len] = PATH_SEPAR;
// 		pfff->path[len + 1] = 0;
// 	}


	pfff->entry.mStartClust = dir->mStartClust;//����ָ����Ŀ¼�ĵ�һ��
	res = PosEntry(&(pfff->entry) , 0);			//��λ��һ��ɽ���ĵ�һ����ڽṹ
	if (res){
		//��λʧ�ܣ������ļ����������
		delete pfff;
		return DR_INVALID_HANDLE;				//��λʧ��
	}

	//���ز��Ҿ��
	*finder = FINDER(pfff);

	return DR_OK;
}

DRES DFat32::FindNextFileW( FINDER finder , DFat32File* file )
{
	PFat32FileFinder pfff = PFat32FileFinder(finder);
	//�豸��û��
	if (0 == strlen(this->mDevName)) return DR_NO_OPEN;
 	//�������
	if (!finder && !file) return DR_INVALED_PARAM;

	//���о���Ĳ���
	if (pfff->isFindDel)
	{//���ҵ����Ѿ�ɾ���˵��ļ�����Ŀ¼
		return FindNextDelFile(finder , file);
	}else{
		//���ҵ�����ͨ�ļ�����Ŀ¼
		return FindNextExistFile(finder , file);
	}

	return DR_OK;
}

DRES DFat32::FindNextExistFile( FINDER find , DFat32File* file )
{
	DRES		res		= DR_OK;
	BYTE*		dir		= NULL;
	BYTE		flag	= 0;		//��һ���ֽڵı�׼
	BYTE		attr	= 0;		//����
	BYTE		chSum	= 0;		//���ļ���У���
	BYTE		order	= 0;		//Ŀ¼�����
	WCHAR		nambuf[MAX_LFN+1] = {0}; 
	int			len		= 0;
	WCHAR		path[MAX_PATH + 1] = {0};
	PFat32FileFinder finder = PFat32FileFinder(find);//��ѯ�ṹ��

	//�����Ѿ�������
	if (finder->isEnd) return DR_FAT_EOF;
	
	
	do{	//����Ŀ¼��ÿһ�����
		res = MoveView(finder->entry.mCurSect);
		if(res)  break;				//����
		dir = finder->entry.mDir;	//��ڵ�λ��
		flag = dir[0];				//��һ���ֽڵı�־λ
		if( flag == 0){				//����Ŀ¼��ĩβ
			res = DR_FAT_EOF;		//���˵��˽�β
			finder->isEnd = TRUE;
			break;		
		}
		
		attr = PSDE(dir)->mAttr & ATTR_FAT32_MASK;//Ŀ¼�������
		
		if (flag == 0xE5||\
			((attr & ATTR_VOLUME_ID) && attr != ATTR_LONG_NAME)) {	//����һ����Ч�����
			order = 0xFF;
		} else {//��һ����Ч�����

			if(attr == ATTR_LONG_NAME){//���ļ������
				if(flag & 0x40)	{	//��һ�����ļ������еĿ�ʼ  Ҳ����һ�����ļ��������һ������
					chSum = PLDE(dir)->mChksum;//�ļ�����У���
					flag &= 0xBF;	//ȥ��0x40������  �ٳ����
					order = flag;	
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���
				}
				//������һ������Ŀ¼�����
				if(order == flag && chSum == PLDE(dir)->mChksum)
				{//ƥ�䵽һ�����ļ���
					--order; 
					SetLFN(nambuf , dir);		//ȡ���ļ���
				}else{//ƥ��ʧ��
					order = 0xFF;
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���
				}
				
			}else{  //һ�����ļ���
				if(!order && chSum == ChkSum(dir)){
				//�ҵ�һ�����ļ���Ŀ¼��Ӧ�Ķ��ļ������

					//��Ҫ�����ļ�·��ƴ��
					wcscpy(path , finder->path);//��·��
					if((len = wcslen(path)) > 1 
						&& !IsPathSeparator(path[len - 1]))     //������ ��Ŀ¼����
						wcscat(path , L"/");	//�ָ���
					wcscat(path , nambuf);		//�ļ���

					//�����ļ������
					NewFileHandle(file , &(finder->entry) , path);
					
					//�Ƚ�����һ�β��ҵ�λ��
					res = NextEntry(&(finder->entry));  //�Ƶ���һ�����
					if (res == DR_FAT_EOF)
						finder->isEnd = TRUE;
					
					return DR_OK;

					//(*listFun)(nambuf);		
				}else{						//����Ķ��ļ������
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���
					order = 0xFF;
					SetSFN(nambuf ,dir);

					//��ҩ�����ļ�·��ƴ��
					wcscpy(path , finder->path);//��·��
					if((len = wcslen(path)) > 1 
						&& !IsPathSeparator(path[len - 1]))     //������ ��Ŀ¼����
						wcscat(path , L"/");	//�ָ���
					wcscat(path , nambuf);		//�ļ���

					//�����ļ����
					NewFileHandle(file , &(finder->entry) , path);

					//�Ƚ�����һ�β��ҵ�λ��
					res = NextEntry(&(finder->entry));  //�Ƶ���һ�����
					if (res == DR_FAT_EOF)
						finder->isEnd = TRUE;
					
					return DR_OK;
				}
			}
		}
		res = NextEntry(&(finder->entry));  //�Ƶ���һ�����
		if (res == DR_FAT_EOF)    //���˴���β��
		{
			finder->isEnd = TRUE;
			return DR_FAT_EOF;
// 			res = DR_FAT_EOF
// /*			(*listFun)(NULL);		//֪ͨ�ص��߲������*/
// 			return DR_OK;
		}
	} while (!res);
	
	return res;
}

DRES DFat32::FindNextDelFile( FINDER find , DFat32File* file )
{
	DRES		res		= DR_OK;	//�������
	BYTE*		dir		= NULL;		//�ڻ����е���ڽṹ
	BYTE		flag	= 0;		//��һ���ֽڵı�׼
	BYTE		attr	= 0;		//����
	USHORT		chSum	= 0xFFFF;	//�����У���Ϊ�����ֽ�
									//��Ҫ����Ϊ�ô������ж��Ƿ���һ���µĳ��ļ����Ŀ�ʼ
									//ӦΪʵ�ʵ�У�����һ���ֽ�,Ҳ����˵����Ϊ0xFF 
									//������0xFFFF����ǰ��û��ƥ�䵽һ������Ŀ¼��
	WCHAR		nambuf[MAX_LFN+1] = {0}; //�ļ�������
	WCHAR       path[MAX_PATH] = {0};	//·������
	DFat32File	df;
	W_CHAR		w_w;					//���ڼ���У���
	PFat32FileFinder finder = PFat32FileFinder(find);

	//�Ѿ����ҽ�����
	if (finder->isEnd) return DR_FAT_EOF;


	do{	//����Ŀ¼��ÿһ�����
		res = MoveView(finder->entry.mCurSect);
		if(res)  break;				//����
		dir = finder->entry.mDir;	//��ڵ�λ��
		flag = dir[0];				//��һ���ֽڵı�־λ
		if( flag == 0){				//����Ŀ¼��ĩβ

			res = DR_FAT_EOF;
			finder->isEnd = TRUE;
			return DR_FAT_EOF;
		}
		attr = PSDE(dir)->mAttr & ATTR_FAT32_MASK;//Ŀ¼�������

		if (flag != 0xE5) {			//����һ��ɾ���˵����
			//order = 0xFF;
			chSum = 0xFFFF;
		} else {//��һ����Ч�����
			if(attr == ATTR_LONG_NAME){//���ļ������

				if(chSum == 0xFFFF)	{
					//��һ�����ļ������еĿ�ʼ
					//Ҳ����һ�����ļ��������һ������
					chSum = PLDE(dir)->mChksum;//��¼��У���
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���
				}
				//�ж�
				if(chSum == PLDE(dir)->mChksum)
				{//ƥ�䵽һ�����ļ���
					AppLFN(nambuf , dir);//ȡ���ļ����������ӵ������ǰ��
				}else{//ƥ��ʧ�� , ����ԭ�е�ƥ����  ������һ���µ�ƥ�����
					//��ջ��� ,������һ��ƥ��
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���
					
					//������һ��ƥ��
					chSum = PLDE(dir)->mChksum;
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���
					AppLFN(nambuf , dir);//ȡ���ļ����������ӵ������ǰ��

				}

			}else{  //һ�����ļ���
				//���ļ�����ڵĵ�һ���ֽ�������0xE5  �Ѿ��޷�����У�����
				//�������ȳ��ļ����л�ԭ�������ټ���
				BYTE btBack = 0;		//���ݵ�һ���ֽ�
				w_w.charw = nambuf[0];
				w_w = ChrConvert(w_w ,FALSE );
				btBack = dir[0];
				if (w_w.char1)//��һ���ַ��Ƕ��ֽ��ַ�
					dir[0] = w_w.char1;
				else		 //��һ���ַ��ǵ��ֽ��ַ�
					dir[0] = w_w.char2;
				
				if(chSum == ChkSum(dir)){

					//�����ļ����
					//����ɾ����־
					wcscpy(path , finder->path);//��·��
					if(wcslen(path) != 1)       //������ ��Ŀ¼����
						wcscat(path , L"/");	//�ָ���
					wcscat(path , L"*");		//ɾ����־
					wcscat(path , nambuf);		//�ļ���

					//��ʼ�����ص��ļ����
					NewFileHandle(file , &(finder->entry) , path);//�������
					chSum = 0xFFFF;			//����У���
					
					//����ǰ������һ�����
					res = NextEntry( &(finder->entry) );  //�Ƶ���һ�����
					if (res == DR_FAT_EOF)    //���˴���β��
						finder->isEnd = TRUE;
					
					dir[0] = btBack;		//��ԭ�����е�����
					return DR_OK;				
// 					res = DR_OK;
// 
// 					break;
					//(*listFun)(df);		//�ҵ�һ�����ļ���Ŀ¼��Ӧ�Ķ��ļ������
				}else{						//����Ķ��ļ������
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���
					
					(*(char*)dir) = '*';//ɾ����־
					SetSFN(nambuf ,dir);

					wcscpy(path , finder->path);//��·��
					if(wcslen(path) != 1)     //������ ��Ŀ¼����
						wcscat(path , L"/");	//�ָ���
					wcscat(path , nambuf);		//�ļ���

					//��ʼ�����ص��ļ����
					NewFileHandle(file , &(finder->entry) , path);//�������
					chSum = 0xFFFF;			//����У���

					
					//����ǰ������һ�����
					res = NextEntry( &(finder->entry) );  //�Ƶ���һ�����
					if (res == DR_FAT_EOF)    //���˴���β��
						finder->isEnd = TRUE;

					dir[0] = btBack;  //��ԭһ������
					return DR_OK;
// 					res = DR_OK;
// 
// 					break;
// 
// 					//����ɾ����־
// /*					(*listFun)(df);		//�ҵ�һ�����ļ���	*/
					
				}
			}
		}
		res = NextEntry( &(finder->entry) );  //�Ƶ���һ�����
		if (res == DR_FAT_EOF)    //���˴���β��
		{
			finder->isEnd = TRUE;
// 			df.mFS = NULL;
// /*			(*listFun)(df);		//֪ͨ�ص��߲������*/
			return DR_FAT_EOF;
		}
	} while (!res);

	return res;
}

void DFat32::FindClose( FINDER finder )
{
	if (finder) delete PFat32FileFinder(finder);
}


DRES DFat32::ListDelFile(DFat32File* fil, FIND_DEL_FILE listFun)
{
//	DFat32File*	fil		= (DFat32File*)(dirc);//�Ѿ��򿪵�Ŀ¼
	DRES		res		= DR_OK;	//�������
	DirEntry	entry	= {0};		//��ڽṹ
	BYTE*		dir		= NULL;		//�ڻ����е���ڽṹ
	BYTE		flag	= 0;		//��һ���ֽڵı�׼
	BYTE		attr	= 0;		//����
	USHORT		chSum	= 0xFFFF;	//�����У���Ϊ�����ֽ�
									//��Ҫ����Ϊ�ô������ж��Ƿ���һ���µĳ��ļ����Ŀ�ʼ
									//ӦΪʵ�ʵ�У�����һ���ֽ�,Ҳ����˵����Ϊ0xFF 
									//������0xFFFF����ǰ��û��ƥ�䵽һ������Ŀ¼��
	WCHAR		nambuf[MAX_LFN+1] = {0}; //�ļ�������
	WCHAR       path[MAX_PATH] = {0};	//·������
	DFat32File	df;
	W_CHAR		w_w;					//���ڼ���У���


	//�豸��û��
	if (0 == strlen(this->mDevName)) return DR_NO_OPEN;

	if(!fil || !listFun )
		return DR_INVALED_PARAM;		//��������
	if(!(fil->mAttr & ATTR_DIRECTORY))
		return DR_INVALED_PARAM;		//��Ҫ����һ��Ŀ¼������һ���ļ�

	entry.mStartClust = fil->mStartClust;//����ָ����Ŀ¼�ĵ�һ��
	res = PosEntry(&entry , 0);			//��λ��һ��ɽ���ĵ�һ����ڽṹ
	if (res) return res;				//��λʧ��

	do{	//����Ŀ¼��ÿһ�����
		res = MoveView(entry.mCurSect);
		if(res)  break;				//����
		dir = entry.mDir;			//��ڵ�λ��
		flag = dir[0];				//��һ���ֽڵı�־λ
		if( flag == 0){				//����Ŀ¼��ĩβ
			df.mFS = NULL;
			(*listFun)(df);		//֪ͨ�ص��߲������
			break;		
		}
		attr = PSDE(dir)->mAttr & ATTR_FAT32_MASK;//Ŀ¼�������

		if (flag != 0xE5) {			//����һ��ɾ���˵����
			//order = 0xFF;
			chSum = 0xFFFF;
		} else {//��һ����Ч�����
			if(attr == ATTR_LONG_NAME){//���ļ������

				if(chSum == 0xFFFF)	{
					//��һ�����ļ������еĿ�ʼ
					//Ҳ����һ�����ļ��������һ������
					chSum = PLDE(dir)->mChksum;//��¼��У���
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���
				}
				//�ж�
				if(chSum == PLDE(dir)->mChksum)
				{//ƥ�䵽һ�����ļ���
					AppLFN(nambuf , dir);//ȡ���ļ����������ӵ������ǰ��
				}else{//ƥ��ʧ��
					//��ջ���
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���
				}

			}else{  //һ�����ļ���
				//���ļ�����ڵĵ�һ���ֽ�������0xE5  �Ѿ��޷�����У�����
				//�������ȳ��ļ����л�ԭ�������ټ���
				w_w.charw = nambuf[0];
				w_w = ChrConvert(w_w ,FALSE );
				if (w_w.char1)//��һ���ַ��Ƕ��ֽ��ַ�
					dir[0] = w_w.char1;
				else		 //��һ���ַ��ǵ��ֽ��ַ�
					dir[0] = w_w.char2;
				
				if(chSum == ChkSum(dir)){

					//�����ļ����
					//����ɾ����־
					wcscpy(path , fil->mPath);//��·��
					if(wcslen(path) != 1)     //������ ��Ŀ¼����
						wcscat(path , L"/");	//�ָ���
					wcscat(path , L"*");		//ɾ����־
					wcscat(path , nambuf);		//�ļ���
					NewFileHandle(&df , &entry , path);//�������

					(*listFun)(df);		//�ҵ�һ�����ļ���Ŀ¼��Ӧ�Ķ��ļ������
					chSum = 0xFFFF;			//����У���
				}else{						//����Ķ��ļ������
					::memset(nambuf , 0 , sizeof(WCHAR)*(MAX_LFN+1));//������ֻ���
					
					(*(char*)dir) = '*';//ɾ����־
					SetSFN(nambuf ,dir);

					wcscpy(path , fil->mPath);//��·��
					if(wcslen(path) != 1)     //������ ��Ŀ¼����
						wcscat(path , L"/");	//�ָ���
					wcscat(path , nambuf);		//�ļ���
					NewFileHandle(&df , &entry , path);//�������

					//����ɾ����־
					(*listFun)(df);		//�ҵ�һ�����ļ���	
					chSum = 0xFFFF;			//����У���
				}
			}
		}
		res = NextEntry(&entry);  //�Ƶ���һ�����
		if (res == DR_FAT_EOF)    //���˴���β��
		{
			df.mFS = NULL;
			(*listFun)(df);		//֪ͨ�ص��߲������
			return DR_OK;
		}
	} while (!res);

	return res;
}

DRES DFat32::NewFileHandle( DFat32File* file, PVOID entr ,const WCHAR* path )
{
	return file->InitFile(entr , path , this);
}
DWORD DFat32::GetSecCount()
{
	return this->mSectors;
}
BYTE DFat32::GetSecPerClust()
{
	return strlen(this->mDevName) != 0 ? this->mSecPerClus : 0 ;
}

DRES DFat32::IsContainFat32Flag(const char* cDevName , LONG_INT offset)
{
	DRES		res  = DR_OK;
	HANDLE		hDev = INVALID_HANDLE_VALUE;
	FAT32_DBR	fDbr = {0};
	DWORD		dwReaded = 0;

	//��������
	if (cDevName == NULL ) return DR_INVALED_PARAM;
	
	//���豸
	hDev = ::CreateFileA(cDevName , GENERIC_READ | GENERIC_WRITE,		 //����ģʽ
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL ,OPEN_EXISTING, 0 ,NULL);
	if (hDev == INVALID_HANDLE_VALUE) //���豸ʧ��
		return DR_OPEN_DEV_ERR;


	if (offset.QuadPart > 0)
	{//ƫ��
		//�����ļ�ָ��
		offset.LowPart = SetFilePointer(hDev , offset.LowPart , PLONG(&(offset.HighPart)) ,FILE_BEGIN );
		if (offset.LowPart == -1 && GetLastError() != NO_ERROR )
			res = DR_DEV_CTRL_ERR;
	}
	
	//��ȡ����
	if(DR_OK == res && !::ReadFile(hDev , &fDbr , sizeof(FAT32_DBR) 
		,&dwReaded ,NULL) && dwReaded != sizeof(FAT32_DBR))	
		res =  DR_DEV_IO_ERR;
	
	//����Ҫ��
	CloseHandle(hDev);
	//�豸��ز���ʧ��
	if (res != DR_OK) return res;
	
	if ( ( fDbr.bsEndSig  != MBR_END)	//dbr������Ǽ�� 
		||(fDbr.bsBytesPerSec != 512)	//ÿ�������ֽ�����512�ı�����һ����512 ��������Ҳ����512�����
		||(fDbr.bsFATs != 2)			//FAT����һ���Ƕ�
		||(fDbr.bsRootDirEnts != 0)		//��Ŀ¼��һ����0  ���ֵ�Ǹ�fat12/16�õ�
		||(fDbr.bsSectors != 0)			//��������  Ҳ��fat12/16�õ�
		||(fDbr.bsFATsecs != 0)			//fat����������  Ҳ��fat12/16�õ�
		||(fDbr.bsHugeSectors == 0))	//һ��fat32����������  һ����λ0
		return DR_NO;

	//���ϵĶ��ǿ��Ժܶ��ģ�Ҳ����˵һ��������FAT32�����ϵ�������������
	//�ɺܿ϶���ֵ����һЩ��������DBR�еĶ�����豸����(fDbr.bsMedia)��
	//��FAT���ĵ�һ���ֽ���һ����ͬ��ֵ,һ����0xF8�����о��Ǹ�Ŀ¼�е�һ
	//����ڵ���һ������Ĵ��λ�ã��϶��Ậ��ATTR_VOLUME_ID(0x08)����

	//���������ж�FAT32��ǣ�ʵ���ϲ��������е�FAT32�������������ǣ���
	//���ǵ���������
	if ( ( fDbr.bsFSType[0] != 0x46)	//F
		||(fDbr.bsFSType[1] != 0x41)	//A
		||(fDbr.bsFSType[2] != 0x54)	//T
		||(fDbr.bsFSType[3] != 0x33)	//3
		||(fDbr.bsFSType[4] != 0x32)	//2
		||(fDbr.bsFSType[5] != 0x20)	//�ո�
		||(fDbr.bsFSType[6] != 0x20)	//�ո�
		||(fDbr.bsFSType[7] != 0x20))	//�ո�
		return DR_NO;

	
	//OK ���ܵ�����Ļ���һ����û�����
	return DR_OK;
}

DRES DFat32::GetVolumeName(char* cNameBuf , int len)
{
	DRES	res		= DR_OK;
	DWORD	offset	= 0;
	UCHAR	buf[SECTOR_SIZE] = {0}; 
	int		i		= 0;
	PSDE	sde		= PSDE(buf);


	//��ǰ�Ƿ����
	if (0 == strlen(this->mDevName)) return DR_NO_OPEN;
	//�������
	if (NULL == cNameBuf) return DR_INVALED_PARAM;
	strcpy(cNameBuf , "");

	//��һ����Ŀ¼��������
	offset = ClustToSect(m1stDirClut);
	res = this->ReadData(buf , offset , SECTOR_SIZE);
	if (DR_OK != res) return res;

	if (sde->mAttr & ATTR_VOLUME_ID)
	{//�������һ������
		//�������ֵĳ���
		for ( i = 0 ; i< 11 && buf[i] != 0x20 ; ++i );
		buf[i] = 0;

		//���治��
		if (len < i)return DR_BUF_OVER;

		//��������
		strcpy(cNameBuf , (char*)buf);	
	}else{
		//��������
		strcpy(cNameBuf , "NO NAME");
		return DR_NO_FILE_NAME;
	}
	return res;
}

const char* DFat32::GetDevName()
{
	//��ǰ�Ƿ����
	if (0 == strlen(this->mDevName))
		return NULL;

	return mDevName;
}

USHORT DFat32::GetReserveSector()
{
	if (0 == strlen(this->mDevName))
		return 0;

	return mResSec;
}

DWORD DFat32::GetSectorPerFAT()
{
	if (0 == strlen(this->mDevName))
		return 0;

	return mSecsPerFAT;
}

USHORT DFat32::GetFSInfoSec()
{
	if (0 == strlen(this->mDevName))
		return 0;

	return mFSinfoSec;
}

DWORD DFat32::Get1stDirClust()
{
	if (0 == strlen(this->mDevName))
		return 0;

	return m1stDirClut;
}


DWORD DFat32::GetRemainSectorCnt()
{
	if (0 == strlen(this->mDevName))
		return 0;

	//����������-��һ��Ŀ¼���ڴغ�������%ÿ��������
	return (mSectors - (mResSec + (mFATs * mSecsPerFAT))) % mSecPerClus;
}


DWORD DFat32::GetMaxClustNum()
{
	if (0 == strlen(this->mDevName))
		return 0;
	return mMaxClust;

}

