#include<stdio.h>
#include<stdlib.h>
#include<string.h>
int main()
{
char *buf;
printf("Content-Type:text/html;charset=utf-8\n\n"); 
printf("\r\n");   
printf("<html>\n");    
printf("<head>\n<title>login test</title>\n</head>\n");    
printf("<body>\n"); 
printf("<h1>welcome to my web server\n");
printf("<p>my cgi demo\n")  ;  
printf("<P>") ;

char* outputsucess={"login sucess"};
char *outputfailed={"login failed"};
char username [50];
char password[50];
char c;
int j=0;
int k=0;;
int n=0;
int flag=0;
int i;
if((buf=getenv("Content-Length"))!=NULL)
{
n=atoi(buf);

  for(i=0;i<n;i++)
   {
    scanf("%c",&c);
   
    if(c=='&')break;
  else {username[j]=c;j++;}
    }
 username[j]='\0';
 i++;
   for(int j=i;j<n;j++)
  { 
    scanf("%c",&c);
    password[j-i]=c;
}
    password[n-i]='\0';
    //printf("%s %s",username,password);
    if((strstr(username,"jc")!=NULL)&&(strstr(password,"123456")!=NULL))
     printf("%s\n",outputsucess);
    else printf("%s\n",outputfailed);
  
 }
else printf("%s\n",outputfailed);
printf("</body>\n");
printf("</html>\n"); 
fflush(stdout);
exit(0);	
}
