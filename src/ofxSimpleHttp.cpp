/*
 *  ofxSimpleHttp.cpp
 *  emptyExample
 *
 *  Created by Oriol Ferrer Mesià on 06/11/10.
 *  Copyright 2010 uri.cat. All rights reserved.
 *
 */


#include "ofxSimpleHttp.h"
#include "ofEvents.h"


ofxSimpleHttp::ofxSimpleHttp(){
	timeOut = 10;
	queueLenEstimation = 0;
	maxQueueLen = 100;
	debug = false;
	timeToStop = false;
	userAgent = "ofxSimpleHttp (Poco Powered)";
	acceptString = "";
}

ofxSimpleHttp::~ofxSimpleHttp(){

	timeToStop = true;	//lets flag the thread so that it doesnt try access stuff while we delete things around
	stopCurrentDownload();

	waitForThread(false);

	//empty queue
	while ( getPendingDownloads() > 0 ){
		lock();
			ofxSimpleHttpResponse * r = q.front();
			delete r;
			q.pop();
		unlock();
	}
}


void ofxSimpleHttp::setTimeOut(int seconds){
	timeOut = seconds;
}


void ofxSimpleHttp::setVerbose(bool verbose){
	debug = verbose;
}


void ofxSimpleHttp::setUserAgent( string newUserAgent ){
	userAgent = newUserAgent;
}


void ofxSimpleHttp::setAcceptString( string newAcceptString ){
	acceptString = newAcceptString;
}


void ofxSimpleHttp::setMaxQueueLenght(int len){
	maxQueueLen = len;
}


void ofxSimpleHttp::threadedFunction(){

	if (debug) printf("\nofxSimpleHttp >> start threadedFunction\n");
	queueLenEstimation = 0;

	lock();
	queueLenEstimation = q.size();
	unlock();

	while( queueLenEstimation > 0 && timeToStop == false){

		lock();
			ofxSimpleHttpResponse * r = q.front();
		unlock();

			downloadURL(r, true);

		lock();
			delete r;
			q.pop();
			queueLenEstimation = q.size();
		unlock();
	}
	//if no more pending requests, let the thread die...
	if (debug) printf("ofxSimpleHttp >> exiting threadedFunction (queue len %d)\n", queueLenEstimation);

	#if  defined(TARGET_OSX) || defined(TARGET_LINUX)
	if (!timeToStop){ //FIXME! TODO
		pthread_detach(pthread_self()); //this is a workaround for this issue https://github.com/openframeworks/openFrameworks/issues/2506
	}
	#endif
}


string ofxSimpleHttp::getCurrentDownloadFileName(){

	string download = "";
	lock();
		int n = q.size();
		if ( isThreadRunning() && n > 0 ){
			ofxSimpleHttpResponse * r = q.front();
			download = ( r->fileName );
		}
	unlock();
	return download;
}


int ofxSimpleHttp::getPendingDownloads(){
	lock();
		queueLenEstimation = q.size();
	unlock();
	return queueLenEstimation;
}


float ofxSimpleHttp::getCurrentDownloadProgress(){

	float downloadPercent = -1;
	lock();
		int n = q.size();
		if ( isThreadRunning() && n > 0){
			ofxSimpleHttpResponse * r = q.front();
			if ( r->serverReportedSize > 0)
				downloadPercent = fabs( (float) ( r->responseBody.size()) / (0.1f + r->serverReportedSize) );
		}
	unlock();
	return downloadPercent;
}


void ofxSimpleHttp::stopCurrentDownload(){

	lock();
		int n = q.size();
		if ( isThreadRunning() && n > 0){
			ofxSimpleHttpResponse * r = q.front();
			if (debug) printf( "ofxSimpleHttp::stopCurrentDownload() >> about to stop download of %s...\n", r->fileName.c_str() );
			try{
				r->downloadCanceled = true;
				if ( r->session != NULL ) r->session->abort();
			}catch(Exception& exc){
				printf( "ofxSimpleHttp::stopCurrentDownload(%s) >> Exception: %s\n", r->fileName.c_str(), exc.displayText().c_str() );
			}
		}
	unlock();
}

void ofxSimpleHttp::draw(float x, float y , float w , float h  ){

	string aux;
	lock();
	int n = q.size();
	if ( isThreadRunning() && n > 0 ){
		ofxSimpleHttpResponse * r = q.front();
		float downloadPercent = fabs( (float) (r->responseBody.size()) / (0.1f + r->serverReportedSize) );
		if ( r->serverReportedSize >= 0)
			aux = "ofxSimpleHttp Now Fetching:\n" + r->url.substr(0, w / 8 ) + "\n" + ofToString(100.0f * downloadPercent,1) + "% done...\nQueue Size " + ofToString(n) ;
		else
			aux = "ofxSimpleHttp Now Fetching:\n" + r->url.substr(0, w / 8) + "\nQueue Size " + ofToString(n);
	}else{
		aux= "ofxSimpleHttp idle...";
	}
	unlock();

//	for(int i = 0; i < aux.length(); i+= w / 8){	//break up the string with \n to fit in supplied width
//		aux.insert(i, "\n");
//	}

	ofSetColor(0,127,255);
	ofDrawBitmapString(aux, x + 3, y + 12 );

}

void ofxSimpleHttp::draw(float x, float y){
	draw(x,y, ofGetWidth() -x, 100);
}


string ofxSimpleHttp::extractFileFromUrl(string url){

	int found = url.find_last_of("/");
	string file = url.substr(found + 1);
	return file;
}


void ofxSimpleHttp::fetchURL(char* url, bool ignoreReply){
	string aux = url;
	fetchURL(aux, ignoreReply);
}


void ofxSimpleHttp::fetchURL(string url, bool ignoreReply){

	if (queueLenEstimation >= maxQueueLen){
		printf( "ofxSimpleHttp::fetchURL can't do that, queue is too long already (%d)!\n", queueLenEstimation );
		return;
	}

	ofxSimpleHttpResponse *response = new ofxSimpleHttpResponse();
	response->url = url;
	response->downloadCanceled = false;
	response->fileName = extractFileFromUrl(url);
	response->ignoreReply = ignoreReply;
	response->session = NULL;

	lock();
		q.push(response);
	unlock();

	if ( !isThreadRunning() ){	//if the queue is not running, lets start it
		startThread(true, false);
	}
}


ofxSimpleHttpResponse ofxSimpleHttp::fetchURLBlocking(char*  url){
	string aux = url;
	return fetchURLBlocking(aux);
}


ofxSimpleHttpResponse ofxSimpleHttp::fetchURLBlocking(string  url){

	response.url = url;
	response.downloadCanceled = false;
	response.session = NULL;
	response.fileName = extractFileFromUrl(url);
	response.ignoreReply = false;
	bool ok = downloadURL(&response, false);
	return response;
}


bool ofxSimpleHttp::downloadURL( ofxSimpleHttpResponse* resp, bool sendResultThroughEvents ){

	resp->ok = FALSE;

	try{

		URI uri( resp->url.c_str() );
		std::string path( uri.getPathAndQuery() );
		if ( path.empty() ) path = "/";

		HTTPClientSession session( uri.getHost(), uri.getPort() );
		resp->session = &session;

		HTTPRequest req( HTTPRequest::HTTP_GET, path, HTTPMessage::HTTP_1_1 );
		req.set( "User-Agent", userAgent.c_str() );
		if (acceptString.length() > 0){
			req.set( "Accept", acceptString.c_str() );
		}
		session.setTimeout( Poco::Timespan(timeOut,0) );
		session.sendRequest( req );
		HTTPResponse res;
		istream& rs = session.receiveResponse(res);

		//fill in the return object
		//resp->url = path;
		resp->status = res.getStatus();
		resp->timestamp = res.getDate();
		resp->reasonForStatus = res.getReasonForStatus( res.getStatus() );
		resp->contentType = res.getContentType();
		resp->serverReportedSize = res.getContentLength();

		if (debug) if (resp->serverReportedSize == -1) printf("ofxSimpleHttp::downloadURL(%s) >> Server doesn't report download size...\n", resp->fileName.c_str() );
		if (debug) printf("ofxSimpleHttp::downloadURL() >> about to start download (%s, %d bytes)\n", resp->fileName.c_str(), res.getContentLength() );
		if (debug) printf("ofxSimpleHttp::downloadURL() >> server reports request staus: (%d-%s)\n", resp->status, resp->reasonForStatus.c_str() );

		if (timeToStop) {
			resp->session = NULL;
			return false;
		};

		try{
			StreamCopier::copyToString(rs, resp->responseBody);	//copy the data...
		}catch(Exception& exc){
			printf("ofxSimpleHttp::downloadURL(%s) >> Exception: %s\n", resp->fileName.c_str(), exc.displayText().c_str() );
			resp->reasonForStatus = exc.displayText();
			resp->ok = false;
			resp->status = -1;
			resp->session = NULL;
			return false;
		}

		if (resp->downloadCanceled){
			if(debug) printf("ofxSimpleHttp::downloadURL() >> download (%s) canceled!\n", resp->fileName.c_str());
			resp->reasonForStatus = "download canceled by user!";
			resp->ok = false;
			resp->status = -1;
			resp->session = NULL;
			return false;
		}

		if(debug) printf("ofxSimpleHttp::downloadURL() >> downloaded (%s)\n", resp->fileName.c_str());

		if ( resp->serverReportedSize > 0 && resp->serverReportedSize != resp->responseBody.size() ) {
			if(debug) printf( "ofxSimpleHttp::downloadURL() >> Download size mismatch (%s) >> Server: %d Downloaded: %d\n",
								resp->fileName.c_str(), resp->serverReportedSize, (int)resp->responseBody.size() );
			resp->reasonForStatus = "Download size mismatch";
			resp->status = -1;
			resp->ok = false;
		}else{
			if (resp->status == 200){
				resp->ok = true;
			}else{
				resp->ok = false;
			}
		}

		if (sendResultThroughEvents ){
			if ( !resp->ignoreReply )
				if (timeToStop == false)	//see if we have been destructed!
					ofNotifyEvent( newResponseEvent, *resp, this );
		}

	}catch(Exception& exc){
		printf("ofxSimpleHttp::downloadURL(%s) >> General Exception: %s\n", resp->url.c_str(), exc.displayText().c_str() );
		resp->ok = FALSE;
		resp->reasonForStatus = exc.displayText();
		resp->status = -1;
		resp->session = NULL;
	}
	resp->session = NULL;
	return resp->ok;
}

