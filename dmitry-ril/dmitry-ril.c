#define LOG_TAG "DMITRY-RIL"
#define RIL_SHLIB
#include <telephony/ril_cdma_sms.h>
#include <sys/system_properties.h>
#include <telephony/librilutils.h>
#include <cutils/sockets.h>
#include <telephony/ril.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <termios.h>
#include <alloca.h>
#include <assert.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include "dmitry-ril.h"

#define REAL_RIL_NAME				"libsec-ril.so"

/* A copy of the original RIL function table. */
static RIL_RadioFunctions const *mRealRadioFuncs;

/* A copy of the ril environment passed to RIL_Init. */
static const struct RIL_Env *mEnv;

//callbacks for android to call
// static void rilOnRequest(int request, void *data, size_t datalen, RIL_Token t)
// {
// 	switch (request) {
// 		case RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING:
// 			//we fake this and never even send it to the real RIL
// 			RLOGW("Faking reply to RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING\n");
// 			mEnv->OnRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
// 			break;
		
// 		case RIL_REQUEST_SIM_OPEN_CHANNEL:
// 			//we fake this and never even send it to the real RIL
// 			RLOGW("Faking reply to RIL_REQUEST_SIM_OPEN_CHANNEL\n");
// 			mEnv->OnRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
// 			break;
		
// 		default:
// 			mRealRadioFuncs->onRequest(request, data, datalen, t);
// 	}
// }

static void onRequestCompleteShim(RIL_Token t, RIL_Errno e, void *response, size_t responselen) {
	int request;
	RequestInfo *pRI;

	pRI = (RequestInfo *)t;

	/* If pRI is null, this entire function is useless. */
	if (pRI == NULL)
		goto null_token_exit;

	request = pRI->pCI->requestNumber;

	switch (request) {
		case RIL_REQUEST_LAST_CALL_FAIL_CAUSE:
			/* Remove extra element (ignored on pre-M, now crashing the framework) */
			if (responselen > sizeof(int)) {
				mEnv->OnRequestComplete(t, e, response, sizeof(int));
				return;
			}
			break;
	}

	RLOGD("%s: got request %s: forwarded to libril.\n", __func__, requestToString(request));
null_token_exit:
	mEnv->OnRequestComplete(t, e, response, responselen);
}

static void patchMem(void *libHandle) {
	/*
	 * MAX_TIMEOUT is used for a call to pthread_cond_timedwait_relative_np.
	 * The issue is bionic has switched to using absolute timeouts instead of
	 * relative timeouts, and a maximum time value can cause an overflow in
	 * the function converting relative to absolute timespecs if unpatched.
	 *
	 * By patching this to 0x01FFFFFF from 0x7FFFFFFF, the timeout should
	 * expire in about a year rather than 68 years, and the RIL should be good
	 * up until the year 2036 or so.
	 */
	uint32_t *MAX_TIMEOUT;

	MAX_TIMEOUT = (uint32_t *)dlsym(libHandle, "MAX_TIMEOUT");
	if (CC_UNLIKELY(!MAX_TIMEOUT)) {
		RLOGE("%s: MAX_TIMEOUT could not be found!", __FUNCTION__);
		return;
	}
	RLOGD("%s: MAX_TIMEOUT found at %p!", __FUNCTION__, MAX_TIMEOUT);
	RLOGD("%s: MAX_TIMEOUT is currently 0x%" PRIX32, __FUNCTION__, *MAX_TIMEOUT);
	if (CC_LIKELY(*MAX_TIMEOUT == 0x7FFFFFFF)) {
		*MAX_TIMEOUT = 0x01FFFFFF;
		RLOGI("%s: MAX_TIMEOUT was changed to 0x0%" PRIX32, __FUNCTION__, *MAX_TIMEOUT);
	} else {
		RLOGW("%s: MAX_TIMEOUT was not 0x7FFFFFFF; leaving alone", __FUNCTION__);
	}

}

const RIL_RadioFunctions* RIL_Init(const struct RIL_Env *env, int argc, char **argv)
{
	RIL_RadioFunctions const* (*fRealRilInit)(const struct RIL_Env *env, int argc, char **argv);
	static RIL_RadioFunctions rilInfo;
	static struct RIL_Env shimmedEnv;
	void *realRilLibHandle;
	int i;

	//save the env;
	mEnv = env;
	shimmedEnv = *env;
	shimmedEnv.OnRequestComplete = onRequestCompleteShim;
	
	//get the real RIL
	realRilLibHandle = dlopen(REAL_RIL_NAME, RTLD_LOCAL);
	if (!realRilLibHandle) {
		RLOGE("Failed to load the real RIL '" REAL_RIL_NAME  "': %s\n", dlerror());
		return NULL;
	}
	
	//remove "-c" command line as Samsung's RIL does not understand it - it just barfs instead
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-c") && i != argc -1) {	//found it
			memcpy(argv + i, argv + i + 2, sizeof(char*[argc - i - 2]));
			argc -= 2;
		}
	}
	
	//load the real RIL
	fRealRilInit = dlsym(realRilLibHandle, "RIL_Init");
	if (!fRealRilInit) {
		RLOGE("Failed to find the real RIL's entry point\n");
		goto out_fail;
	}

	RLOGD("Calling the real RIL's entry point with %u args\n", argc);
	for (i = 0; i < argc; i++)
		RLOGD("  argv[%2d] = '%s'\n", i, argv[i]);

	// Fix RIL issues by patching memory
	patchMem(realRilLibHandle);

	//try to init the real ril
	mRealRadioFuncs = fRealRilInit(&shimmedEnv, argc, argv);
	if (!mRealRadioFuncs) {
		RLOGE("The real RIL's entry point failed\n");
		goto out_fail;
	}
	
	//copy the real RIL's info struct, then replace the onRequest pointer with our own
	rilInfo = *mRealRadioFuncs;
	// rilInfo.onRequest = rilOnRequest;

	//show the real RIl's version
	RLOGD("Real RIL version is '%s'\n", mRealRadioFuncs->getVersion());
	
	RLOGI("P4 RIL interposition library by me@dmitry.gr loaded\n");
	ALOGI("P4 RIL interposition library by me@dmitry.gr loaded\n");
	
	//we're all good - return to caller
	return &rilInfo;

out_fail:
	dlclose(realRilLibHandle);
	return NULL;
}