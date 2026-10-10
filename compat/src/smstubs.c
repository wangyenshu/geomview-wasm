/*
 * smstubs.c -- libSM/libICE stand-ins for libXt.  A browser page has no X
 * session manager, so SmcOpenConnection() always fails and Xt's
 * SessionShell simply runs without session management.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <X11/SM/SMlib.h>

SmcConn
SmcOpenConnection(char *networkIdsList, SmPointer context, int xsmpMajorRev,
	int xsmpMinorRev, unsigned long mask, SmcCallbacks *callbacks,
	const char *previousId, char **clientIdRet, int errorLength,
	char *errorStringRet)
{
    if (clientIdRet) *clientIdRet = NULL;
    if (errorStringRet && errorLength > 0) {
	snprintf(errorStringRet, errorLength, "no session manager in browser");
    }
    return NULL;
}

SmcCloseStatus SmcCloseConnection(SmcConn c, int n, char **r)
	{ return SmcConnectionInUse; }
void SmcModifyCallbacks(SmcConn c, unsigned long m, SmcCallbacks *cb) { }
void SmcSetProperties(SmcConn c, int n, SmProp **p) { }
void SmcDeleteProperties(SmcConn c, int n, char **p) { }
Status SmcInteractRequest(SmcConn c, int t, SmcInteractProc p, SmPointer d)
	{ return 0; }
void SmcInteractDone(SmcConn c, Bool cancel) { }
void SmcSaveYourselfDone(SmcConn c, Bool success) { }
Status SmcRequestSaveYourselfPhase2(SmcConn c, SmcSaveYourselfPhase2Proc p,
	SmPointer d) { return 0; }
char *SmcClientID(SmcConn c) { return NULL; }
IceConn SmcGetIceConnection(SmcConn c) { return NULL; }
int IceConnectionNumber(IceConn c) { return -1; }
IceProcessMessagesStatus IceProcessMessages(IceConn c,
	IceReplyWaitInfo *w, Bool *r) { return IceProcessMessagesIOError; }
