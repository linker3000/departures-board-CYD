/*
 * Departures Board (c) 2025 Gadec Software
 * 
 * raildataXmlClient Library
 * 
 * https://github.com/gadec-uk/departures-board
 * 
 * This work is licensed under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/ 
 */

#pragma once
#include <xmlListener.h>
#include <xmlStreamingParser.h>

typedef void (*rdCallback) (int state, int id);

#define MAXBOARDSERVICES 8
#define MAXBOARDMESSAGES 4
#define MAXMESSAGESIZE 400
#define MAXLOCATIONSIZE 45

#define OTHER 0
#define TRAIN 1
#define BUS 2

#define UPD_SUCCESS 0
#define UPD_INCOMPLETE 1
#define UPD_UNAUTHORISED 2
#define UPD_HTTP_ERROR 3
#define UPD_TIMEOUT 4
#define UPD_NO_RESPONSE 5
#define UPD_DATA_ERROR 6
#define UPD_NO_CHANGE 7

#define MAXHOSTSIZE 48
#define MAXAPIURLSIZE 48

struct rdService {
  char sTime[6];
  char destination[MAXLOCATIONSIZE];
  char via[MAXLOCATIONSIZE];
  char origin[MAXLOCATIONSIZE];
  char etd[11];
  char platform[4];
  bool isCancelled;
  bool isDelayed;
  int trainLength;
  byte classesAvailable;
  char opco[50];
  char calling[MAXMESSAGESIZE];
  char serviceMessage[MAXMESSAGESIZE];
  int serviceType;
};

struct rdStation {
  char location[MAXLOCATIONSIZE];
  bool platformAvailable;
  int numServices;
  int numMessages;
  rdService service[MAXBOARDSERVICES];
  char nrccMessages[MAXBOARDMESSAGES][MAXMESSAGESIZE];
};

class raildataXmlClient: public xmlListener {

    private:

        String grandParentTagName = "";
        String parentTagName = "";
        String tagName = "";
        String tagPath = "";
        int tagLevel = 0;
        bool loadingWDSL=false;
        String soapURL = "";
        char soapHost[MAXHOSTSIZE];
        char soapAPI[MAXAPIURLSIZE];        

        String currentPath = "";
        rdStation xStation;
        bool addedStopLocation = false;
        int id=0;
        int coaches=0;
        char buffer[MAXMESSAGESIZE];

        String lastErrorMessage = "";
        bool firstDataLoad;
        bool endXml;

        rdCallback Xcb;
        void removeHtmlTags(char* input);
        void replaceWord(char* input, const char* target, const char* replacement);
        void pruneFromPhrase(char* input, const char* target);
        void fixFullStop(char* input);
        void sanitiseData();
        void deleteService(int x);

        virtual void startTag(const char *tagName);
        virtual void endTag(const char *tagName);
        virtual void parameter(const char *param);
        virtual void value(const char *value);
        virtual void attribute(const char *attribute); 

    public: 
        raildataXmlClient();
        int init(const char *wsdlHost, const char *wsdlAPI, rdCallback RDcb);
        int updateDepartures(rdStation *station, const char *crsCode, const char *customToken, int numRows, bool includeBusServices);
        String getLastError();
};