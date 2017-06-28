#include<stdio.h>
#include<stdlib.h>
#define maxline 100

int main()
{
char *buf;
char *output="this is a cgi test";
int n=0;
if((buf=getenv("QUERY_STRING"))!=NULL)
{
n=atoi(buf);

}
if(n>=1&&n<100)
for(int i=1;i<=n;i++)
{
  printf("%s.......\n",output);
}
else printf("test\n");
exit(0);
}
