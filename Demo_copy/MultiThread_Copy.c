#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<fcntl.h>
#include<string.h>
#include<pthread.h>
#include<sys/mman.h>
char * ptr=NULL;
int fsize = 0;

//线程复制所有用到的信息结构体
struct file_info{
	const char * dfile;
	int blocksize;
	int pos;

};
//检测命令行参数
void Check_arg(int argno,const char * sfile,int threadno){
	
	if(argno <3 || argno > 4){
		printf("error:argno less or more...\n");	
	}
	if(access(sfile,F_OK) != 0){
		printf("error:file no exit...\n");	
	}
	if(threadno < 1 || threadno >100){
		printf("error:threadno less or more...\n");	
	}
	
}
//文件私有映射和切块
int Get_Blocksize(const char * sfile,int threadno){
	
	int fd;
	fd = open(sfile,O_RDONLY);
	fsize = lseek(fd,0,SEEK_END);
	if((ptr = mmap(NULL,fsize,PROT_READ,MAP_PRIVATE,fd,0)) == MAP_FAILED){
		perror("mmap fail:");
		exit(0);
	}
	close(fd);
	if(fsize % threadno == 0){
		return fsize/threadno;	
	}else{
		return fsize/threadno + 1;	
	}
}
void * thread_copy(void * arg){
	struct file_info info = *(struct file_info *)arg;
	//printf("thread pos:%d blocksize:%d\n",info.pos,info.blocksize);
	int fd = open(info.dfile,O_WRONLY | O_CREAT,0664);
	if(info.pos + info.blocksize > fsize){
		info.blocksize = fsize % info.blocksize;	
	}
	//老子不拷了，用那个都拷不全，直接从映射内存那里用指针偏移进行复制，哼哼，不愧是我，哈哈哈！！！
	//char buf[info.blocksize];
	//bzero(buf,info.blocksize);
	//snprintf(buf,info.blocksize+1,"%s",ptr+info.pos);
	//strncpy(buf,ptr+info.pos,info.blocksize);
	//printf("bufsize:%d,buf:%s\n",strlen(buf),buf);
	
	//这里别忘记对目标文件的光标进行偏移
	lseek(fd,info.pos,SEEK_SET);
	int wsize = write(fd,ptr+info.pos,info.blocksize);
	//printf("write size:%d\n",wsize);
	close(fd);
	pthread_exit(NULL);
}
void * pthread_job_bar(void * arg){
	usleep(200);
	const char * dfile = (const char *)arg;
	double rsize = 0;//在目标文件中读到的文件大小
	int fd = open(dfile,O_RDONLY);
	rsize = lseek(fd,0,SEEK_END);//根据偏移到文件尾部的偏移量获取当前文件大小
	double per = rsize/fsize;//文件复制进度
	int num = 0;//进度条中#数量
	int temp =0;//记录上一次读入的文件大小
	while(per < 1){
		
		
		rsize = lseek(fd,0,SEEK_END);
		per = rsize/fsize;
		//这里是因为多开了一个线程进行复制，导致目标文件大小大于原文件大小
		//防止输出110%
		//if(per > 1){
		//	break;
		//}
		//防止输出相同的结果
		if(temp != rsize){
			temp = rsize;	
		}else{
			continue;	
		}
		//ftruncate(STDOUT_FILENO,0);	
		//printf("It's already been copied:%.2lf\%\n",rsize/fsize*100);
		//进度条设计
		num = per *50;
		printf("复制文件进度：[");
		for(int i =0;i<50;i++){
			if(i<num){
				printf(">");	
			}else{
				printf(" ");	
			}
		}
		printf("]%% %.2f\n",per*100);

	}
	printf("复制成功！！！\n");
	close(fd);
	pthread_exit(NULL);
}

void Create_Thread(const char * sfile ,const char *dfile,int threadno,int blocksize){
	pthread_t tid[threadno];
	//更改为分离态就无法判断什么时候线程都退出了，就无法回收映射空间（回收早导致复制失败，回首晚浪费时间）
	//pthread_attr_t Attr;
	//pthread_attr_init(&Attr);
	//pthread_attr_setdetachstate(&Attr,PTHREAD_CREATE_DETACHED);
	//printf("fsize:%d",fsize);
	int err;
	struct file_info info;
	info.dfile = dfile;
	info.blocksize = blocksize;
	int i = 0;

	//创建进度条进程
	if((err = pthread_create(&tid[i],NULL,pthread_job_bar,(void *)dfile)) > 0){
		printf("thread create fail:%s\n",strerror(err));
		exit(0);

	}
	//把i++扔到错误处理里了，导致多创建了一个线程，我说怎么目标文件总比源文件大呢
	i++;
	//创建复制线程
	for(;i <= threadno;i++){
		info.pos = (i-1)*blocksize;

		if((err = pthread_create(&tid[i],NULL,thread_copy,(void *)&info)) > 0){
			printf("thread create fail:%s\n",strerror(err));
			exit(0);
		}
		usleep(100);
	}
	//线程回收
	while(i--){

		if((err = pthread_join(tid[i],NULL))>0){
			printf("%d join fail:%s\n",i,strerror(err));	
		}	
	}

}


int main(int argc,char ** argv){
	
	int threadno = 10;
	if(argc != 3){
		threadno = atoi(argv[3]);
	}
	//检查命令行参数
	Check_arg(argc,argv[1],threadno);
	//获取块大小，并完成文件的私有映射
	int blocksize = Get_Blocksize(argv[1],threadno);
	//线程创建和回收
	Create_Thread(argv[1],argv[2],threadno,blocksize);
	munmap(ptr,fsize);
	printf("done\n");
	return 0;
}
