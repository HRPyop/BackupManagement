#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define PATH_SIZE 256
#define FILE_SIZE 256
#define COMMAND_SIZE 128

struct backup{
	char fname[PATH_SIZE];
	int period;
	int option[4];
	pthread_t tid;
	struct backup *next;
};
struct backup_file{
	char fname[PATH_SIZE];
	int num;
	struct backup_file *next;
};

int log_fd;
char dir_path[PATH_SIZE];
char cur_path[PATH_SIZE];
struct backup_file *backup_head=NULL;

void print_usage(char *name);
struct backup *add_new_backuplist(char *name,int period,struct backup *head);
struct backup *remove_backuplist(char *name,struct backup *head);
void added_log(char *name);
void removed_log(char *name);
void recovered_log(char *name);
void generated_log(char *name);
void add_backup_file(char *fname,int num);
void remove_backup_file(char *fname);
void *backup_thread(void *arg);

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc,char *argv[])
{
	char command[COMMAND_SIZE];
	struct stat dir_stat;
	struct backup *head = (struct backup *) malloc(sizeof(struct backup));
	head->next = NULL;

	getcwd(cur_path,PATH_SIZE);

	if(argc>2) //too many parameter
		print_usage(argv[0]);
	else if(argc == 1){ //make backup directory 
		mkdir("backup",0777);
		chdir("backup");
		getcwd(dir_path,PATH_SIZE);
		log_fd = creat("log_file.txt",0644);
		chdir(cur_path);
	}
	else if(argc == 2){ 
		stat(argv[1],&dir_stat);
		if(!S_ISDIR(dir_stat.st_mode)) //argv[1] is not directory
			print_usage(argv[0]);
		if(access(argv[1],X_OK)<0) //can't access directory
			print_usage(argv[0]);
		if(chdir(argv[1])<0) //can't find directory
			print_usage(argv[0]);
		else{
			getcwd(dir_path,PATH_SIZE);
			log_fd = creat("log_file.txt",0644);
			chdir(cur_path);
		}
	}
	while(1){
		printf("20162524>");
		command[0] = 0;
		scanf("%[^\n]",command);
		getchar();//remove \n in buffer
		if(!strncmp(command,"exit",4))
			break;
		else if(!strncmp(command,"add",3)){
			char *name;
			struct stat tmp_stat;
			struct dirent *dentry;
			DIR *dirp;
			int mflag=0;
			char *n_option;
			int nflag = 0;

			if(strstr(command,"-m")!=NULL)
				mflag = 1;
			if((n_option = strstr(command,"-n"))!=NULL){
				n_option = strchr(n_option,' ');
				n_option++;
				nflag = atoi(n_option);
				if(nflag==0){
					fprintf(stderr,"not use of n option number\n");
					continue;
				}
			}

			if(strlen(command)<=4){
				fprintf(stderr,"add error!\nusage:add <filename>[period][option]\n");
				continue;
			}
			char *ptr = strchr(command,' ');
			ptr++;

			//-d option used
			if(strstr(ptr,"-d")!=NULL){
				name = ptr;
				if((ptr = strchr(ptr,' '))==NULL){
					fprintf(stderr,"not use of period\n");
					continue;
				}
				*ptr = 0;
				ptr++;
				int period = atoi(ptr);
				if(period == 0){
					fprintf(stderr,"not use of period\n");
					continue;
				}
				stat(name,&tmp_stat);
				if(!S_ISDIR(tmp_stat.st_mode)){
					fprintf(stderr,"%s is not directory\n",name);
					continue;
				}
				if((dirp = opendir(name))==NULL||chdir(name)==-1){
					fprintf(stderr,"opendir,chdir error\n");
					continue;
				}
				while((dentry = readdir(dirp))!=NULL){
					if(!strcmp(dentry->d_name,"..")||!strcmp(dentry->d_name,"."))
						continue;
					lstat(dentry->d_name,&tmp_stat);
					if(!S_ISREG(tmp_stat.st_mode))
						continue;

					head = add_new_backuplist(dentry->d_name,period,head);//add to linked list
					
					head->option[3] = 1;//d option used
					if(mflag)
						head->option[0] = 1;//m option used
					if(nflag)
						head->option[1] = nflag;//n option used
					
					added_log(head->fname);
					
					if(pthread_create(&head->tid,NULL,backup_thread,(void*)head)!=0){//create thread to backup
						fprintf(stderr,"thread create error\n");
						continue;
					}
					pthread_detach(head->tid);//wait for thread end
				}
				chdir(cur_path);
			}

			//normal add
			else{
				name = ptr;
				if((ptr = strchr(ptr,' ')) == NULL){
					fprintf(stderr,"not use of period\n");
					continue;
				}
				*ptr = 0;
				ptr++;
				stat(name,&tmp_stat);
				if(!S_ISREG(tmp_stat.st_mode)){
					fprintf(stderr,"%s is not regular file\n",name);
					continue;
				}
				if(access(name,F_OK)<0){
					fprintf(stderr,"%s is not exist\n",name);
					continue;
				}
				if(atoi(ptr)==0){
					fprintf(stderr,"not use of period\n");
					continue;
				}

				head = add_new_backuplist(name,atoi(ptr),head);//add to linked list
				added_log(head->fname);
				
				if(mflag)//m option used
					head->option[0] = 1;
				if(nflag)//n option used
					head->option[1] = nflag;
				if(pthread_create(&head->tid,NULL,backup_thread,(void*)head)!=0){//create thread to backup
					fprintf(stderr,"thread create error\n");
					continue;
				}
				pthread_detach(head->tid);//wait for thread end
			}
		}

		else if(!strncmp(command,"list",4)){//print backup list
			struct backup *print = head;
			while(print->next!=NULL){
				printf("%s\t%d\t",print->fname,print->period);
				//print used option
				if(print->option[0])
					printf("m");
				if(print->option[1])
					printf("n");
				if(print->option[2])
					printf("t");
				if(print->option[3])
					printf("d");
				printf("\n");
				print = print->next;
			}
		}

		else if(!strncmp(command,"remove",6)){
			char *name;
			char backup_name[PATH_SIZE];
	
			if((name = strchr(command,' '))==NULL){
				fprintf(stderr,"not use file name to remove\n");
				continue;
			}
			name++;
			if(!strcmp(name,"-a")){//remove all list
				while(head->next!=NULL){
					removed_log(head->fname);
					pthread_cancel(head->tid);
					head = head->next;
				}
				backup_head = NULL;
			}
			else{
				head = remove_backuplist(name,head);//remove file from list

				if(strchr(name,'/')!=NULL){
					name = strchr(name,'/');
					name++;
				}
				sprintf(backup_name,"%s/%s",dir_path,name);
				
				//remove backuped file from list
				pthread_mutex_lock(&mutex);
				struct backup_file *remove = backup_head;
				struct backup_file *prev = NULL;
				while(remove != NULL){
					if(!strncmp(remove->fname,backup_name,strlen(backup_name))){
						if(prev == NULL)
							backup_head = remove ->next;
						else
							prev -> next = remove->next;
						remove = remove ->next;
						continue;
					}
					prev = remove;
					remove = remove ->next;
				}
				pthread_mutex_unlock(&mutex);
			}
		}

		else if(!strncmp(command,"compare",7)){
			char *name1,*name2;
			struct stat buf1,buf2;

			//get files name
			if((name1 = strchr(command,' ')) == NULL){
				fprintf(stderr,"usage:compare <file1> <file2>\n");
				continue;
			}
			name1++;
			if((name2 = strchr(name1,' '))==NULL){
				fprintf(stderr,"usage: compare <file1> <file2>\n");
				continue;
			}
			*name2 = 0;
			name2++;

			//get stat of files
			lstat(name1,&buf1);
			lstat(name2,&buf2);

			if((!S_ISREG(buf1.st_mode))||(!S_ISREG(buf2.st_mode))){
				fprintf(stderr,"use regular file\n");
				continue;
			}
			if((access(name1,F_OK)<0)||(access(name2,F_OK)<0)){
				fprintf(stderr,"file not exist\n");
				continue;
			}
			

			if((buf1.st_mtime==buf2.st_mtime)&&(buf1.st_size==buf2.st_size))
				printf("%s and %s if same file!\n",name1,name2);
			else{
				printf("%s mtime: %ld size: %ld\n",name1,buf1.st_mtime,buf1.st_size);
				printf("%s mtime: %ld size: %ld\n",name2,buf2.st_mtime,buf2.st_size);
			}
		}
		else if(!strncmp(command,"recover",7)){
			char *name;
			char fname[PATH_SIZE];
			char recovered_name[PATH_SIZE];
			if((name = strchr(command,' '))==NULL){
				fprintf(stderr,"usage: recover <filename> [option]\n");
				continue;
			}
			name++;
			char *ptr;
			int nflag=0;
			if(strstr(command,"-n")!=NULL){
				ptr = strchr(name,' ');
				*ptr = 0;
				nflag = 1;
			}

			if(strstr(name,"/")!=NULL){
				strcpy(fname,dir_path);
				strcat(fname,strrchr(name,'/'));
			}
			else{
				strcpy(fname,dir_path);
				strcat(fname,"/");
				strcat(fname,name);
			}

			if(nflag){//set recover name
				ptr++;
				ptr = strchr(ptr,' ');
				ptr++;
				strcpy(recovered_name,ptr);
			}
			else{
				if(!strncmp(name,".",1)){
					char *temp = name+1;
					strcpy(recovered_name,getcwd(NULL,0));
					strcat(recovered_name,temp);
				}
				else if(strncmp(name,"/",1)){
					strcpy(recovered_name,getcwd(NULL,0));
					strcat(recovered_name,"/");
					strcat(recovered_name,name);
				}
				else	
					strcpy(recovered_name,name);
			}

			pthread_mutex_lock(&mutex);
			struct backup_file *recover = backup_head;
			int num;
			struct stat buf;
			printf("0. exit\n");
			while(recover!=NULL){//print recoverable files
				if(!strncmp(recover->fname,fname,strlen(fname))){
					stat(recover->fname,&buf);
					printf("%d.%s\t%ldbytes\n",recover->num,strrchr(recover->fname,'_')+1,buf.st_size);
				}
				recover = recover->next;
			}

			scanf("%d",&num);
			getchar();
			if(num==0){
				pthread_mutex_unlock(&mutex);
				continue;
			}
			else{
				recover = backup_head;
				while(recover->next != NULL){
					if(!strncmp(recover->fname,fname,strlen(fname))){
						if(recover->num==num){
							int fd_new = open(recovered_name,O_WRONLY|O_CREAT|O_TRUNC,0644);
							int fd_old = open(recover->fname,O_RDONLY);
							int length;
							char buf[BUFSIZ];
							
							while((length = read(fd_old,buf,BUFSIZ))>0)
								write(fd_new,buf,length);

							close(fd_new);
							close(fd_old);

							recovered_log(recovered_name);
							break;
						}
					}
					recover = recover->next;
				}
			}
			pthread_mutex_unlock(&mutex);
		}
		else if(!strncmp(command,"ls",2))
			system(command);
		else if(!strncmp(command,"vim",3)||!strncmp(command,"vi",2))
			system(command);
	
	}
	exit(0);
}
void print_usage(char *name)
{
	fprintf(stderr,"usage: %s <directory>\n",name);
	exit(1);
}

struct backup *add_new_backuplist(char *name,int period,struct backup *head)
{
	struct backup *new = (struct backup *)malloc(sizeof(struct backup));
	if(head ==NULL)
		head = (struct backup*)malloc(sizeof(struct backup));

	if(!strncmp(name,".",1)){
		char *temp = name+1;
		strcpy(new->fname,getcwd(NULL,0));
		strcat(new->fname,temp);
	}
	else if(strncmp(name,"/",1)){
		strcpy(new->fname,getcwd(NULL,0));
		strcat(new->fname,"/");
		strcat(new->fname,name);
	}
	else	
		strcpy(new->fname,name);
	new->period = period;
	new->option[0] = 0;
	new->option[1] = 0;
	new->option[2] = 0;
	new->option[3] = 0;
	new->next = head;
	head = new;
	return head;
}

struct backup *remove_backuplist(char *name,struct backup *head)
{
	struct backup *remove = head;
	struct backup *prev = NULL;
	char finding_name[FILE_SIZE];

	if(strncmp(name,"/",1)){
		strcpy(finding_name,getcwd(NULL,0));
		strcat(finding_name,"/");
		strcat(finding_name,name);
	}
	else
		strcpy(finding_name,name);

	while(remove->next!=NULL){
		if(!strcmp(remove->fname,finding_name)){
			if(prev==NULL)
				head = remove->next;
			else
				prev->next = remove->next;
			removed_log(remove->fname);
			pthread_cancel(remove->tid);
			return head;
		}
		prev = remove;
		remove = remove->next;
	}
	printf("can't find remove file\n");
	return head;
}

void added_log(char *name)
{
	time_t curtime;
	time(&curtime);
	struct tm *tm_ptr;
	tm_ptr = gmtime(&curtime);

	char buf[BUFSIZ];
	sprintf(buf,"[%02d%02d%02d %02d%02d%02d] %s added\n",tm_ptr->tm_year-100,tm_ptr->tm_mon+1,tm_ptr->tm_mday,tm_ptr->tm_hour,tm_ptr->tm_min,tm_ptr->tm_sec,name);

	pthread_mutex_lock(&mutex);
	write(log_fd,buf,strlen(buf));
	pthread_mutex_unlock(&mutex);
}

void removed_log(char *name)
{
	time_t curtime;
	time(&curtime);
	struct tm *tm_ptr;
	tm_ptr = gmtime(&curtime);

	char buf[BUFSIZ];
	sprintf(buf,"[%02d%02d%02d %02d%02d%02d] %s deleted\n",tm_ptr->tm_year-100,tm_ptr->tm_mon+1,tm_ptr->tm_mday,tm_ptr->tm_hour,tm_ptr->tm_min,tm_ptr->tm_sec,name);
	
	pthread_mutex_lock(&mutex);
	write(log_fd,buf,strlen(buf));
	pthread_mutex_unlock(&mutex);
}
void recovered_log(char *name)
{
	time_t curtime;
	time(&curtime);
	struct tm *tm_ptr;
	tm_ptr = gmtime(&curtime);

	char buf[BUFSIZ];
	sprintf(buf,"[%02d%02d%02d %02d%02d%02d] %s recovered\n",tm_ptr->tm_year-100,tm_ptr->tm_mon+1,tm_ptr->tm_mday,tm_ptr->tm_hour,tm_ptr->tm_min,tm_ptr->tm_sec,name);

	write(log_fd,buf,strlen(buf));
}
void add_backup_file(char *fname,int num)
{
	struct backup_file *new = (struct backup_file*)malloc(sizeof(struct backup_file));
	strcpy(new->fname,fname);
	new->num = num;
	new->next = NULL;
	struct backup_file *tail = backup_head;
	if(tail == NULL){
		backup_head = (struct backup_file *)malloc(sizeof(struct backup_file));
		backup_head = new;
		pthread_mutex_unlock(&mutex);
		return ;
	}
	while(tail->next!=NULL)
		tail = tail->next;
	tail ->next = new;
}
void remove_backup_file(char *fname)
{
	pthread_mutex_lock(&mutex);
	struct backup_file *cur = backup_head;
	struct backup_file *prev = NULL;
	while(cur!=NULL){
		if(!strncmp(cur->fname,fname,strlen(fname))){
			if(cur->num==1){
				char sysbuf[BUFSIZ];
				sprintf(sysbuf,"rm %s",cur->fname);
				system(sysbuf);
				if(prev==NULL)
					backup_head = cur->next;
				else
					prev->next = cur->next;
				break;
			}
		}
		prev = cur;
		cur = cur->next;
	}
	pthread_mutex_unlock(&mutex);

	removed_log(cur->fname);
	
	pthread_mutex_lock(&mutex);
	while(cur!=NULL){
		if(!strncmp(cur->fname,fname,strlen(fname)))
			cur->num--;
		cur= cur->next;
	}
	pthread_mutex_unlock(&mutex);
}
void *backup_thread(void *arg)
{
	struct backup *existing = (struct backup*)arg;
	time_t cur_time;
	struct tm *tm_ptr;
	char backup_file[BUFSIZ];
	char buf[BUFSIZ];
	char generated_log[BUFSIZ];
	int fd,backup_fd;
	int length;
	int count = 1;
	struct stat statbuf;
	time_t mtime;
	char name[BUFSIZ];

	sprintf(name,"%s%s",dir_path,strrchr(existing->fname,'/'));
	
	stat(existing->fname,&statbuf);
	mtime = statbuf.st_mtime;

	while(1){
		sleep(existing->period);
		char *fname = strrchr(existing->fname,'/');
		
		if(existing->option[0]){
			stat(existing->fname,&statbuf);
			if(mtime == statbuf.st_mtime){
				continue;
			}
			else
				mtime = statbuf.st_mtime;
		}
		if((existing->option[1]!=0)&&(count > existing->option[1])){
			remove_backup_file(name);
			count--;
			continue;
		}
			
		time(&cur_time);
		tm_ptr = gmtime(&cur_time);


		sprintf(backup_file,"%s%s_%02d%02d%02d%02d%02d%02d",dir_path,fname,tm_ptr->tm_year-100,tm_ptr->tm_mon+1,tm_ptr->tm_mday,tm_ptr->tm_hour,tm_ptr->tm_min,tm_ptr->tm_sec);
		pthread_mutex_lock(&mutex);
		
		if((fd = open(existing->fname,O_RDONLY))<0){
			fprintf(stderr,"open error\n");
		}
		if((backup_fd = creat(backup_file,0644))<0){
			fprintf(stderr,"file create error\n");
		}

		while((length = read(fd,buf,BUFSIZ))>0)
			write(backup_fd,buf,length);

		sprintf(generated_log,"[%02d%02d%02d %02d%02d%02d] %s generated\n",tm_ptr->tm_year-100,tm_ptr->tm_mon+1,tm_ptr->tm_mday,tm_ptr->tm_hour,tm_ptr->tm_min,tm_ptr->tm_sec,backup_file);
	
		close(fd);
		close(backup_fd);

		add_backup_file(backup_file,count++);
		write(log_fd,generated_log,strlen(generated_log));
		pthread_mutex_unlock(&mutex);
	}
}
