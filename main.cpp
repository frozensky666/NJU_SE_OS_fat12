#include<string.h>
#include<stdarg.h>
#include<stdlib.h>
#include<iostream>

using namespace std;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

int  BytsPerSec;	//每扇区字节数
int  SecPerClus;	//每簇扇区数
int  RsvdSecCnt;	//Boot记录占用的扇区数
int  NumFATs;	//FAT表个数
int  RootEntCnt;	//根目录最大文件数
int  FATSz;	//FAT扇区数

//各种偏移量
int  BOOT;  //根目录偏移，为0
int  FAT1;  //FAT1偏移
int  ROOTENTRY; //根目录偏移
int  DATA;  //数据区偏移

FILE *fat12; //指向img的指针

#pragma pack(1) //1字节对齐

//从11字节处开始读,读取13字节
typedef struct {
    u16  BPB_BytsPerSec;	//每扇区字节数
	u8   BPB_SecPerClus;	//每簇扇区数
	u16  BPB_RsvdSecCnt;	//Boot记录占用的扇区数
	u8   BPB_NumFATs;	//FAT表个数
	u16  BPB_RootEntCnt;	//根目录最大文件数
	u16  BPB_TotSec16;
	u8   BPB_Media;
	u16  BPB_FATSz16;	//FAT扇区数
    u16  BPB_SecPerTrk;
	u16  BPB_NumHeads;
	u32  BPB_HiddSec;
	u32  BPB_TotSec32;	//如果BPB_FATSz16为0，该值为FAT扇区数
}BPB;

//根目录条目，读取32字节
typedef struct {
    char DIR_Name[11];
	u8   DIR_Attr;		//文件属性
	u8   NT_reserved;
    u8   creationTime_In_Tenths_Of_Secoond;
    u16  creationTime;
    u16  creationDate;
    u16  lastAccessDate;
    u16  noUse;
	u16  DIR_ModTime;
	u16  DIR_ModDate;
	u16  DIR_FstClus;	//开始簇号
	u32  DIR_FileSize;
}RootEntry;

#pragma pack() //取消指定对齐，回复缺省对齐（8字节对齐）

//Declare

int getNextFat(FILE *fat12,int first);
void fillBPB(FILE*, BPB*);
int getClusN_offset(int);
void cat(char*);
bool analysisPath(char*,char(*)[12]);
char* fileNameFormat(char*,char*);
void ls_core(int,bool,char*);
int get_offset(int &,int,char [][12],int,bool &);
void my_ls(char*,bool);
void handle_commands();
void showFileCount(int);
void colorPrint(const char *, ...);
bool isNull(RootEntry *r);
void my_printf(const char* buf,int len);//NASM
int getClusFromOffset(int offset);

//Main
int main(int argc, char* argv[]){
    // if(argc==1)return 0;
    // char *filesys=argv[1];
    char filesys[]="ref.img";

    //Load image
    BPB bpb;
    BPB *bpb_ptr = &bpb;
    fat12 = fopen(filesys,"rb");
    fillBPB(fat12,bpb_ptr);

    //全局变量
    BytsPerSec = bpb_ptr->BPB_BytsPerSec;
    SecPerClus = bpb_ptr->BPB_SecPerClus;
    RsvdSecCnt = bpb_ptr->BPB_RsvdSecCnt;
    NumFATs = bpb_ptr->BPB_NumFATs;
    RootEntCnt = bpb_ptr->BPB_RootEntCnt;
    if(bpb_ptr->BPB_FATSz16)FATSz = bpb_ptr->BPB_FATSz16;
    else FATSz = bpb_ptr->BPB_TotSec32;

    //offsets
    BOOT = 0;
    FAT1 = BytsPerSec*RsvdSecCnt;
    ROOTENTRY = FAT1+NumFATs*FATSz*BytsPerSec;
    DATA = ROOTENTRY+RootEntCnt*32;

    //Handle_Commands
    handle_commands();
    
    return 0;
}

//Func

void handle_commands(){//Handle commands
    char command[100];
    char filepath[100];
    bool fileFlag;

    colorPrint(">");
    while(cin.getline(command,99)){//cin.getline(command,99)
        fileFlag=false;
        bool isValid=true;

        //Exit
        if(!strcmp(command,"exit"))break;   

        char* p;
        p=strtok(command," ");

        //Handle ls
        if(p&&!strcmp(p,"ls")){
            bool details = false;
            while(p=strtok(NULL," ")){
                if(*p!='-'){
                    if(!fileFlag){
                        strcpy(filepath,p);
                        fileFlag = !fileFlag;
                    }
                    else {
                        isValid=false;break;
                    }
                }else{
                    p++;
                    if(!*p){
                        isValid=false;break;
                    }
                    while(*p=='l')p++;
                    if(!(*p)){
                        details = true;
                    }else{
                        isValid=false;break;
                    }
                }
            }
            if(isValid){//没有文件参数，代表根目录
                if (!fileFlag){
                    filepath[0]='/';
                    filepath[1]='\0';
                }
                my_ls(filepath,details);
            }else{
                colorPrint("Invalid command!\n");
            }
       }

       //Handle cat
       else if(p&&!strcmp(p,"cat")){
            while(p=strtok(NULL," ")){
                if(fileFlag){
                    isValid=false;break;
                }
                strcpy(filepath,p);
                fileFlag=true;
            }
            if(isValid&&fileFlag){
                cat(filepath);
            }else{
                colorPrint("Invalid command!\n");
            }
       }
       
       //Handle else
       else{
            colorPrint("Invalid command!\n");
       }
       colorPrint(">");
    }
}

void my_ls(char* filepath,bool details){
    char path[100][12];
    bool IsDir_path = analysisPath(filepath,path);
    bool IsDir_real;
    int offset_last;
    int offset = get_offset(offset_last,ROOTENTRY,path,0,IsDir_real);
    if((IsDir_path&&!IsDir_real&&path[0][0]!=0)||offset==-1)
        colorPrint("%s","No such file or directory!\n");
    else if(IsDir_real)
        ls_core(offset,details,filepath);
    else {
        // colorPrint("%s",filepath);
        // if(details){
        //     RootEntry root,*r_ptr = &root;
        //     fseek(fat12,offset_last ,SEEK_SET);
        //     fread(r_ptr,1,32,fat12);
        //     colorPrint(" %d\n",r_ptr->DIR_FileSize);
        // }else{
        //     colorPrint("\n");
        // }
        colorPrint("Invalid command!\n");
    }
}

void ls_core(int offset,bool details,char* whole_path){
    RootEntry root,*r_ptr = &root;
    char tmppath[12]={0};
    fseek(fat12,offset ,SEEK_SET);
    fread(r_ptr,1,32,fat12);
    if(whole_path[strlen(whole_path)-1]!='/')
        strcat(whole_path,"/");
    colorPrint("%s",whole_path);
    if(details)showFileCount(offset);
    colorPrint(":\n");
    for(int i=0;;i++){ //打印当前目录下文件
        fseek(fat12,offset+32*i ,SEEK_SET);
        fread(r_ptr,1,32,fat12);
        if(isNull(r_ptr))break;
        if(r_ptr->DIR_Name[0]!='\345'){
            fileNameFormat(tmppath,r_ptr->DIR_Name);
            bool isDir = r_ptr->DIR_Attr & 0x10;
            if(isDir)colorPrint("\033[40;31m%s\033[0m",tmppath);
            else colorPrint("%s",tmppath);
            if(details){
                if(r_ptr->DIR_Name[0]!='.'){
                    if(isDir)
                        showFileCount(getClusN_offset(r_ptr->DIR_FstClus));
                    else
                        colorPrint(" %d",r_ptr->DIR_FileSize);
                }
                colorPrint("\n");
            }else{
                colorPrint(" ");
            }
        }
    }
    colorPrint("\n");
    for(int i=0;;i++){ //进入子目录，进行递归
        fseek(fat12,offset+32*i ,SEEK_SET);
        fread(r_ptr,1,32,fat12);
        if(isNull(r_ptr))break;
        if((r_ptr->DIR_Attr & 0x10) && (*(u8*)r_ptr!=0xe5) && r_ptr->DIR_Name[0]!='.'){
            char tmp_whole_path[100];
            strcpy(tmp_whole_path,whole_path);
            ls_core(getClusN_offset(r_ptr->DIR_FstClus),details,
                strcat( strcat( tmp_whole_path,fileNameFormat(tmppath,r_ptr->DIR_Name) ),"/" ) );
        }
    }
}

void showFileCount(int offset){
    RootEntry root,*r_ptr = &root;
    int fileCount=0,dirCount=0;
    for(int i=0;;i++){
        fseek(fat12,offset+32*i ,SEEK_SET);
        fread(r_ptr,1,32,fat12);
        if(isNull(r_ptr))break;
        if((*(u8*)r_ptr!=0xe5) && r_ptr->DIR_Name[0]!='.'){
            if(r_ptr->DIR_Attr & 0x10)dirCount++;
            else fileCount++;
        }
    }
    colorPrint(" %d %d",dirCount,fileCount);
}

int get_offset(int &offset_last,int offset,char path[][12],int index,bool &isDir){
    if(path[0][0]==0)isDir=true;
    if(path[index][0]==0)
        return offset;
    else {
        RootEntry root,*r_ptr = &root;
        char tmppath[12]={0};
        for(int i=0;;i++){
            fseek(fat12,offset+32*i ,SEEK_SET);
            fread(r_ptr,1,32,fat12);
            if(isNull(r_ptr))return -1;
            if((*(u8*)r_ptr!=0xe5) && !strcmp(path[index],fileNameFormat(tmppath,r_ptr->DIR_Name))){
                return get_offset(offset_last=offset+32*i,getClusN_offset(r_ptr->DIR_FstClus),path,++index,isDir=r_ptr->DIR_Attr & 0x10);
            }
        }
        return -1;
    }
}

void cat(char* filepath){
    char path[100][12];
    if(analysisPath(filepath,path)){
        colorPrint("'%s' is not a file!\n",filepath);
        return;
    }
    int offset_last;
    bool isDir;
    int offset = get_offset(offset_last,ROOTENTRY,path,0,isDir);
    if(offset==-1){
        colorPrint("No such file!\n");
        return;
    }else if(isDir){
        colorPrint("'%s/' is not a file!\n",filepath);
        return;
    }else{
        char buf[513]={0};
        u32 clus;
        fseek(fat12,offset,SEEK_SET);
        fread(buf,512,1,fat12);
        colorPrint("%s\n",buf); 
        while((clus=getNextFat(fat12,getClusFromOffset(offset)))<0xFF8){
            if(clus==0xFF7){
                colorPrint("坏簇!\n");
                break;
            }
            fseek(fat12,offset=getClusN_offset(clus),SEEK_SET);
            fread(buf,512,1,fat12);
            colorPrint("%s\n",buf);
        }
        
    }
}

char* fileNameFormat(char* f_after,char* f_before){
    strncpy(f_after,f_before,8);
    char *p=f_after+7;
    while(*p==' ')*p--='\0';
    if(*(f_before+8)!=' '){
        *(++p)='.';*(p+4)='\0';
        for(int i=3;i>0;i--)
            *(p+i)=*(f_before+7+i)==' '?'\0':*(f_before+7+i);
    }
    else *(++p)='\0';
    return f_after;
}

bool analysisPath(char* filepath,char path[][12]){//返回值为true，表示是文件夹；false为文件
    char *p;
    char filePath[100];
    strcpy(filePath,filepath);
    p = strtok(filePath,"/");
    while(p){
        strcpy(*(path++),p);
        p = strtok(NULL,"/");
    }
    strcpy(*path,"");
    return filepath[strlen(filepath)-1]=='/';
}

void fillBPB(FILE * fat12 , BPB* bpb_ptr){
    int check;
    check = fseek(fat12,11,SEEK_SET);
    if(check==-1)printf("fseek in fillBPB failed!!");
    check = fread(bpb_ptr,1,13,fat12);
    if(check!=13)printf("fread in fillBPB failed!");
}

int getClusN_offset(int n){
    return DATA+(n-2)*SecPerClus*BytsPerSec;
}

int getClusFromOffset(int offset){
    return (offset-DATA)/SecPerClus/BytsPerSec+2;
}

void colorPrint(const char * format_str, ...){
    va_list argptr;
    char buf[1000];
    va_start(argptr, format_str);
    vsprintf(buf, format_str, argptr);
    my_printf(buf, strlen(buf));
}

// void my_printf(const char* buf,int len){
//     printf("%s",buf);
// }

bool isNull(RootEntry *r){
    return *(int*)r==0;
}

int getNextFat(FILE *fat12,int first){//fisrt当前簇号;
    fseek(fat12,FAT1+first*3/2,SEEK_SET);
    u16 res;
    fread(&res,1,2,fat12);
    res=first%2==0?res<<4 :res;
    return res>>4;
}