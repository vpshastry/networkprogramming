#include "header.h"

void get_ip_of_vm(char vmno_str[6], char* final_ip, int length){

  struct hostent *hptr;
  char **pptr;
  char str [INET_ADDRSTRLEN];
  if ( (hptr = gethostbyname (vmno_str) ) == NULL) {
      err_msg ("gethostbyname error for host: %s: %s",
               vmno_str, hstrerror (h_errno) );
      exit(0);
  }
  //printf ("The VM IP address is ");
  switch (hptr->h_addrtype) {
  case AF_INET:
      pptr = hptr->h_addr_list;
      for (; *pptr != NULL; pptr++) {
          Inet_ntop(hptr->h_addrtype, *pptr, str, sizeof (str));
          strncpy(final_ip, str, length - 1);
          final_ip[length - 1] = '\0';
      }
      break;
  default:
      err_ret ("unknown address type");
      break;
  }

  //return final_ip;
}

void print_tour_list(tour_list_node_t* tour_list_node, int length) {
  int i;
  for (i = 0; i < length; i++) {
    printf("%s\n", tour_list_node[i].node_ip);
  }
}

void make_list(int argc, char* argv[]) {
  
  char vm_ip[MAX_IP_LEN], my_hostname[MAXLINE];
  int i;
  tour_list_node_t tour_list_node[argc];
  for (i = 1; i < argc; i++) {
    get_ip_of_vm(argv[i], vm_ip, MAX_IP_LEN);
    printf("%s: %s\n", argv[i], vm_ip);
    strcpy(tour_list_node[i].node_ip, vm_ip);
    tour_list_node[i].is_cur = NO;
  }
  gethostname(my_hostname, MAXLINE);
  printf("My hostname: %s\n", my_hostname);
  get_ip_of_vm(my_hostname, vm_ip, MAX_IP_LEN);
  strcpy(tour_list_node[0].node_ip, vm_ip);
  tour_list_node[0].is_cur = YES;
  print_tour_list(tour_list_node, argc);
} 

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <list of vms>\n", argv[0]);
    exit(0);
  }
  make_list(argc, argv);
  return 0;
}
