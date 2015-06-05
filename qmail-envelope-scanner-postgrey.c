/* qrey.c
 * an c frontend to postgrey to use primary whith qmail
 *
 * depends on Kelly French <kfrench at federalhill.net> patch for using
 * CHECKENV, qmail-checkenv.diff
 *
 * Written by Anton Lundin <glance@acc.umu.se>
 *
 * Licence: GPL 2+
 */

/*
 * A postfix smtpd_access_policy call whoud look like this:
 * and it is the
 *
 * request=smtpd_access_policy
 * sender=foo@bar.tld
 * recipient=bar@foo.tld
 * client_address=1.2.3.4
 * client_name=another.domain.tld
 *
 * <lines below are not in intresst for postgrey>
 *
 * protocol_state=RCPT
 * protocol_name=SMTP
 * helo_name=some.domain.tld
 * queue_id=8045F2AB23
 * instance=123.456.7
 * sasl_method=plain
 * sasl_username=you
 * sasl_sender=
 * size=12345
 * [empty line]
 *
 * Then the answer looks like, if greylisted
 *
 * action=DEFER_IF_PERMIT Greylisted for 300 seconds (see http://isg.ee.ethz.ch/tools/postgrey/help/foo.tld.html)
 * [empty line]
 *
 * and if the mail shud be let thrue it is like
 * action=DUNNO
 * [empty line]
 *
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define request                 "request=smtpd_access_policy"
#define reply                   "Greylisted (see http://projects.puremagic.com/greylisting/)"
#define ANSWER_GREYLIST         "DEFER_IF_PERMIT"
#define ANSWER_GO               "DUNNO"
#define QUERY_SIZE              1024
#define EXIT_GO                 0   /* good to go, accept email */
#define EXIT_PERM               100 /* permanent reject of mail */
#define EXIT_TEMP               111 /* tempfail, aka graylist */
#define EXIT_ERROR              -1  /* something went wrong */

/* config*/
#define POSTGREY    "localhost" /*where it is running, gethostbyname notation*/
#define PORT        60000       /* port to connect to postgrey on.*/
/* end config */

int main(int argc, char *argv[]) {
    int sock, size;
    struct sockaddr_in saddr;
    struct hostent *hp;
    char query[QUERY_SIZE],answer[32];
    char *remote_address,*remote_name, *sender, *recipient;

    remote_name = getenv("TCPREMOTEHOST");
    remote_address = getenv("TCPREMOTEIP");
    if ((argc != 3 ) || remote_address == NULL) {
        fprintf(stderr,"usage:  qmail-checkenv-postgrey mailfrom.s addr.s\n");
        if ( remote_address == NULL) {
            fprintf(stderr,"called in wrong enviorment, RTFS!\n");
        }
        exit(EXIT_ERROR);
    } else {
        if (remote_name == NULL) {
            remote_name = remote_address;
        }
        sender = argv[1];
        recipient = argv[2];
    }

    /* init so we have all the structs for connect */
    bzero(&saddr, sizeof (saddr));
    saddr.sin_family = AF_INET;

    if ((hp = gethostbyname(POSTGREY)) == NULL) {
        perror("gethostbyname");
        return EXIT_ERROR;
    }


    bcopy(hp->h_addr, &saddr.sin_addr, hp->h_length);
    saddr.sin_port = htons(PORT);


    /* get a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        perror("socket");
        return EXIT_ERROR;
    }

    /* connect the socket to the other end */
    if (connect(sock, (struct sockaddr *)&saddr, sizeof (saddr)) != 0) {
        perror("connect");
        return EXIT_ERROR;
    }

    /* check so the query not gets to log */
    if (( strlen(request) + strlen(remote_address) + strlen(remote_name)
        + strlen(sender) + strlen(recipient)) >= (QUERY_SIZE - 77)) {
        /* 77 is
echo "request=smtpd_access_policysender=recipient=client_address=client_name=     " | wc
        */
        fprintf(stderr,"%s: To long query!\n",argv[0]);
        exit(EXIT_ERROR);
    }
    /* build the query */
    sprintf(query,"%s\nclient_address=%s\nclient_name=%s\nsender=%s\necipient=%s\n\n" ,request,remote_address,remote_name,sender,recipient);
    /* send the query */
    if ( send(sock,&query,strlen(query),0) != strlen(query)) {
        perror("send");
        exit(EXIT_ERROR);
    }

    /* recive the answer from postgrey */
    size = recv(sock, &query, strlen(query), 0);

    close(sock);
    /* pase out the action=? */
    if (sscanf(query,"action=%31s",answer) < 1 ) {
        perror("sscanf");
        exit(EXIT_ERROR);
    }

    /* figure out how to exit */
    if ( strcmp(answer,ANSWER_GREYLIST) == 0) {
        /* is greylisted */
        fprintf(stdout,"%s\n",reply);
        exit(EXIT_TEMP);
    } else if ( strcmp(answer,ANSWER_GO) == 0) {
        /* is good to go */
        exit(EXIT_GO);
    } else {
        fprintf(stderr,"Postgrey answer strange... : %s\n",query);
        exit(EXIT_ERROR);
    }

    /* shud not happen, but anyway. */
    exit(EXIT_GO);
}
