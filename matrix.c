#include <u.h>
#include <libc.h>
#include <bio.h>
#include <json.h>
#define CHUNKSIZE 5

static int conn, ctlfd;
static char *mtpt;

int
httpcode(char *errstr)
{
	return atoi(strstr(errstr, " "));
}

char*
mget(char *suburl)
{
	char *got, *url;
	Biobufhdr *buff;

	url = smprint("url %s", suburl);
	if(suburl[0] != '\0')	/* in case of empty suburl, result of POST is returned, due to webfs behavior */
		if(fprint(ctlfd, url) <= 0)
			sysfatal("write error: %r");

	if((buff = Bopen(smprint("%s/%d/body", mtpt, conn), OREAD)) == nil)
		return nil;

	got = Brdstr(buff, '\n', 1);
	Bterm(buff);
	return got;		/* needs freeing */
}

char*
mpost(char *suburl, char *msg)
{
	int fd;

	if(fprint(ctlfd, "url %s", suburl) <= 0)
		sysfatal("write error: %r");
	if((fd = open(smprint("%s/%d/postbody", mtpt, conn), OWRITE)) < 0){
		print("%s", smprint("%r"));
		return nil;
	}
	if(fprint(fd, msg) <= 0)
		sysfatal("post failed: %r");
	close(fd);
	return mget("");
}

int
numoccs(JSON *arr, char *var, char *val)
{
	if(arr == nil)
		return -1;
	JSONEl *e, *o;
	int ct;

	e = arr->first;
	for(ct = 0; e != nil; e = e->next){
		o = e->val->first;
		if(strcmp(o->name, var) == 0 && strcmp(jsonstr(o->val), val) == 0)
			ct++;
	}
	return ct;
}

JSON*
login(char *user, char *pass)
{
	char *body;
	JSON *j;

	if((mpost("login", smprint("{\"type\":\"m.login.password\", \"user\":\"%s\", \"password\":\"%s\"}", user, pass))) == nil)
		return nil;
	if((j = jsonparse(body)) == nil)
		return nil;
	return j;
}

void
usage(void)
{
	fprint(2, "usage: %s [-s homeserver] [-m webfs mtpt] [-u username] [-p password]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	char buf[256], *body, *mserv, *user, *pass, *usrid, *devid, *rms, *atok;
	JSON *jbuf;

	mtpt = "/mnt/web";
	mserv = "https://matrix.org";
	user = "mushchlo";
	pass = "Checker$3141";

	ARGBEGIN{
	default:
		usage();
	case 's':
		mserv = EARGF(usage());
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	case 'u':
		user = EARGF(usage());
		break;
	case 'p':
		pass = EARGF(usage());
		break;
	}ARGEND;

	if((ctlfd = open(smprint("%s/clone", mtpt), ORDWR)) < 0)
		sysfatal("could not open: %r");
	if(read(ctlfd, buf, sizeof(buf)) <= 0)
		sysfatal("could not read: %r");
	conn = atoi(buf);

	if(fprint(ctlfd, "baseurl %s/_matrix/client/r0/", mserv) <= 0)
		sysfatal("write error: %r");

	body = mget("login");
	jbuf = jsonparse(body);
	free(body);
	if(numoccs(jsonbyname(jbuf, "flows"), "type", "m.login.password") <= 0)
		sysfatal("server %s does not support password login, supports methods: %s", mserv, body);

	if((jbuf = login(user, pass)) == nil)
		sysfatal("incorrect login");
	usrid = jsonstr(jsonbyname(jbuf, "user_id"));
	atok = jsonstr(jsonbyname(jbuf, "access_token"));

//	print("usr id is '%s', generated rmid '', atok '%s'\n", usrid, atok);

	body = mget(smprint("joined_rooms?access_token=%s", atok));
	jbuf = jsonparse(body);
	free(body);
	rms = jsonstr(jsonbyname(jbuf, "room_id"));

//	print("usr id is '%s', queries '%s', atok '%s'\n", usrid, body, atok);

	jsonfree(jbuf);
	close(ctlfd);
	exits(nil);
}
