/*
 * Departures Board (c) 2025 Gadec Software
 *
 * TfL London Underground Client Library
 *
 * https://github.com/gadec-uk/departures-board
 *
 * This work is licensed under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/
 */
#pragma once
#include <JsonListener.h>
#include <JsonStreamingParser.h>
#include <stationData.h>

typedef void (*tflClientCallback) ();

#define MAXLINESIZE 20
#define UGMAXREADSERVICES 20

class TfLdataClient: public JsonListener {

    private:

        struct ugService {
            char destinationName[MAXLOCATIONSIZE];
            char lineName[MAXLINESIZE];
            int timeToStation;
        };

        struct ugStation {
            int numServices;
            ugService service[UGMAXREADSERVICES];
        };

        const char* apiHost = "api.tfl.gov.uk";
        String currentKey = "";
        String currentObject = "";

        int id=0;
        bool maxServicesRead = false;
        ugStation xStation;
        stnMessages xMessages;

        //tflClientCallback Xcb;
        bool pruneFromPhrase(char* input, const char* target);
        static bool compareTimes(const ugService& a, const ugService& b);

    public:
        String lastErrorMsg = "";

        TfLdataClient();
        int updateArrivals(rdStation *station, stnMessages *messages, const char *locationId, String apiKey, tflClientCallback Xcb);

        virtual void whitespace(char c);
        virtual void startDocument();
        virtual void key(String key);
        virtual void value(String value);
        virtual void endArray();
        virtual void endObject();
        virtual void endDocument();
        virtual void startArray();
        virtual void startObject();
};