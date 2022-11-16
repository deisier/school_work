#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<pthread.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>



//这个比较简单，只让j动，i不动所以j可以根据i再调整过来。
int * IntNext(char * ch){

	int len=strlen(ch);
	//new一个数组存前后缀的最大匹配数
	//int * next=new int[len];
	int * next=(int *)malloc(sizeof(int)*len);
	next[0]=0;
	int i=1;//遍历的下标
	int j=i-1;//进行匹配对比的下标
	while(i<len){
		if(ch[i] == ch[next[j]]){
			next[i]=next[j]+1;
			i++;
			j=i-1;
		}else if(next[j] == 0){
			next[i] = 0;
			i++;
			j=i-1;
		}else{
            j= next[j]-1;
		}	
	}
	return next;
}


//主串，匹配串
int match(char * ch1,char *ch2,int ch1_size){
	int len1 = ch1_size;
	int len2 = strlen(ch2);
	int *next = IntNext(ch2);
    
	
	int i=0;
	int j=0;

	while(i<len1 && j<len2){
		if(ch1[i] == ch2[j]){
			i++;
			j++;
		}else{
			if(j == 0){
				i++;
			}else{
				j=next[j-1];
			}
		}
	}
	free(next);
	if(j == len2){
		return 1;
	}else{
		return 0;
	}	
}

#define SIZE 1024

pthread_mutex_t lock1,lock2;
pthread_cond_t cd0, cd1,cd2,cd3;
char buf_A[SIZE];
char buf_B[SIZE];
int sizeBuf_A = 0;//记录一行的位置
int sizeBuf_B = 0;
int fsize = 0;//文件偏移量
int flag = 0; //退出标识，为1,都退出

void *job_A(void * arg){
	printf("job_A start\n");
	pthread_detach(pthread_self());
	int fd;
	if((fd= open("ERROR.log",O_RDONLY)) == -1){
		perror("ERROR file open fail:");
		exit(0);
	}
	int recvsize = 0;
	do{
		pthread_mutex_lock(&lock1);
		while(sizeBuf_A != 0){
			pthread_cond_wait(&cd0,&lock1);	
		}
		recvsize = read(fd,buf_A,SIZE);
		int i = 0;
		//获取换行位置
		for(i;i<recvsize;i++) if(*(buf_A+i) == '\n') {i++;break;}	
		//记录一行的大小
		sizeBuf_A = i++;
		//设置偏移量到下一行的起始位置
		fsize +=sizeBuf_A;
		lseek(fd,fsize,SEEK_SET);
		//打印此次获取的行大小以及目前扫描位置
		printf("job_A work end sizebufA:%d fsize:%d\n",sizeBuf_A,fsize);
		pthread_mutex_unlock(&lock1);
		pthread_cond_signal(&cd1);
	}while(sizeBuf_A != 0);
	flag = 1;
	pthread_cond_signal(&cd1);
	printf("job_A work end\n");
	//线程退出
	pthread_exit(NULL);
		
}
void *job_B(void * arg){
	
	printf("job_B start\n");
	pthread_detach(pthread_self());
	
	while(1){
		//第一把锁，锁bufA
		pthread_mutex_lock(&lock1);
		pthread_mutex_lock(&lock2);
		while(sizeBuf_A == 0){
			pthread_cond_wait(&cd1,&lock1);
			if(flag == 1){
				printf("job_B work end\n");
				pthread_mutex_unlock(&lock2);
				pthread_cond_signal(&cd3);
				pthread_exit(NULL);
			}
		}
		while(sizeBuf_B != 0){
		//	printf("---\n");
			pthread_cond_wait(&cd2,&lock2);	
		}
		//printf("++++\n");	
		if(sizeBuf_A == 0)
		printf("match running \n");
		//匹配成功就写入
		if(match(buf_A,"E CamX",sizeBuf_A) || match(buf_A,"CHIUSECASE",sizeBuf_A)){
			//write(fd,buf_A,sizeBuf_A);
			for(int i = 0;i<sizeBuf_A;i++){
				buf_B[i] = buf_A[i];	
			}
			sizeBuf_B = sizeBuf_A;
			pthread_mutex_unlock(&lock2);
			pthread_cond_signal(&cd3);
		}
		printf("match end \n");
		//bufA置零
		sizeBuf_A = 0;
		pthread_mutex_unlock(&lock2);
		pthread_mutex_unlock(&lock1);
		pthread_cond_signal(&cd0);
	}
}
void * job_C(void * arg){
	
	
	printf("job_C start\n");
	pthread_detach(pthread_self());
	int fd;
	if((fd= open("goal.txt",O_RDWR|O_CREAT,0664)) == -1){
		perror("goal file open fail:");
		exit(0);
	}
	while(1){
		pthread_mutex_lock(&lock2);
		while(sizeBuf_B == 0){
			pthread_cond_wait(&cd3,&lock2);
			if(flag == 1){
				printf("job_c work end\n");
				pthread_exit(NULL);
			}
		}
		printf("job_c start write:%d...\n",fsize);
		write(fd,buf_B,sizeBuf_B);
		printf("job_c write end...\n");
		sizeBuf_B = 0;
		pthread_mutex_unlock(&lock2);
		pthread_cond_signal(&cd2);
		
	}
	
}


int main(){
	
	if(pthread_mutex_init(&lock1,NULL)!= 0 && pthread_mutex_init(&lock2,NULL)!= 0 && pthread_cond_init(&cd1,NULL) && pthread_cond_init(&cd2,NULL) && pthread_cond_init(&cd0,NULL) && pthread_cond_init(&cd3,NULL)){
		perror("lock or cond init fail:");
		exit(0);
	}
	int err = 0;
	pthread_t tid[3];
	if((err = pthread_create(&tid[0],NULL,job_A,NULL)) > 0){
		printf("tid0 create fail:%s\n",strerror(err));
		exit(0);
	}

	if((err = pthread_create(&tid[1],NULL,job_B,NULL)) > 0){
		printf("tid1 create fail:%s\n",strerror(err));
		exit(0);
	}

	if((err = pthread_create(&tid[2],NULL,job_C,NULL)) > 0){
		printf("tid2 create fail:%s\n",strerror(err));
		exit(0);
	}
	while(1)
		sleep(1);
	return 0;
}
