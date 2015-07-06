\datethis
@s ipv4a int
@s u32 int
@s route int
@s table_entry int
@s subtable int
@s bool int

@* Introduction. This is a hastily written interactive interface so that I can
make rudimentary tests on the {\mc IPRT} subroutines.

@c
#include "iprt.h"
char buf[1024];
extern route null_route;

main()
{
  register char *p;
  initialize();
  while (1) {
    printf("\niprt> "); fflush(stdout);
    if (!fgets(buf,1024,stdin)) buf[0]='q';
    for (p=buf; *p && *p!='\n'; p++) ;
    *p='\0';
    p=buf;    
    switch(*p) {
case 'd': @<Delete a route@>;@+break; /* |"d<prefix>"| or |"d<name>"| */
case 'i': @<Insert a route@>;@+break; /* |"i<prefix><name>"| */
case 'l': @<Lookup an IP address@>;@+break; /* |"l<hex>"| */
case 'r': @<Print all routes@>;@+break; /* |"r"| */
case 's': @<Print stats of a subtable@>;@+break; /* |"s<hex>"| */
case 't': @<Print entries of a subtable@>;@+break; /* |"t<hex> <k> <levs>"| */
case 'q': exit(0);
default: printf("Eh? Please see the documentation.");@+break;
    }
  }
}

@ @<Print all routes@>=
print_routes();

@ @<Lookup an IP address@>=
{
  ipv4a a;
  if (sscanf(p+1,"%x",&a)==1) printf(find_match(a)->id);
  else printf("I couldn't read '%s' as a hex number!",p+1);
}


@ @<Insert a route@>=
{
  register ipv4a dest=0,mask=0;
  for (p++; *p=='0' || *p=='1'; p++)
    dest=(dest<<1)+*p-'0', mask=(mask<<1)+1;
  while ((mask&0x80000000)==0) dest<<=1, mask<<=1;
  while (*p==' ') p++;
  if (!insert_route(dest,mask,p)) printf("That route was already there!");
}

@ @<Delete a route@>=
{
  register route *r;
  register ipv4a dest=0,mask=0;
  if (*p=='0' || *p=='1') {
    for (p++; *p=='0' || *p=='1'; p++)
      dest=(dest<<1)+*p-'0', mask=(mask<<1)+1;
    while ((mask&0x80000000)==0) dest<<=1, mask<<=1;
  } else for (r=null_route.next;r!=&null_route;r=r->next)
    if (strncmp(r->id,buf+1,7)==0) {
      dest=r->dest, mask=r->mask;@+ break;
    }  
  if (!mask) printf("Sorry, I couldn't find route %s!",buf+1);
  else if (!delete_route(dest,mask)) printf("That route wasn't present!");
}

@ @<Print stats of a subtable@>=
{
  u32 u;
  if (sscanf(p+1,"%x",&u)==1) print_stats((subtable)(u&-8));
  else printf("I couldn't read '%s' as a hex number!",p+1);
}

@ @<Print entries of a subtable@>=
{
  u32 u;
  int k,levs;
  if (sscanf(p+1,"%x %d %d",&u,&k,&levs)!=3)
    printf("I couldn't read '%s' as <hex> <int> <levs>!",p+1);
  else {
    if (!u) u=(u32)&root_table;
    print_entries((subtable)(u&-8),k,levs);
  }
}

@* Index.
