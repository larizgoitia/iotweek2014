#include "HttpClient.h"

#define LOGGING
#define TIMEOUT 5000 // Allow maximum 5s between data packets.

/**
* Constructor.
*/
HttpClient::HttpClient()
{

}

/**
* Method to send a header, should only be called from within the class.
*/
void HttpClient::sendHeader(const char* aHeaderName, const char* aHeaderValue)
{
    client.print(aHeaderName);
    client.print(": ");
    client.println(aHeaderValue);

    #ifdef LOGGING
    Serial.print(aHeaderName);
    Serial.print(": ");
    Serial.println(aHeaderValue);
    #endif
}

void HttpClient::sendHeader(const char* aHeaderName, const int aHeaderValue)
{
    client.print(aHeaderName);
    client.print(": ");
    client.println(aHeaderValue);

    #ifdef LOGGING
    Serial.print(aHeaderName);
    Serial.print(": ");
    Serial.println(aHeaderValue);
    #endif
}

void HttpClient::sendHeader(const char* aHeaderName)
{
    client.println(aHeaderName);

    #ifdef LOGGING
    Serial.println(aHeaderName);
    #endif
}

/**
* Method to send an HTTP Request. Allocate variables in your application code
* in the aResponse struct and set the headers and the options in the aRequest
* struct.
*/
void HttpClient::request(http_request_t &aRequest, http_response_t &aResponse, http_header_t headers[], const char* aHttpMethod)
{
    // If a proper response code isn't received it will be set to -1.
    aResponse.status = -1;

    bool connected = client.connect(aRequest.hostname.c_str(), aRequest.port);
    if (!connected) {
        client.stop();
        // If TCP Client can't connect to host, exit here.
        return;
    }

    #ifdef LOGGING
    if (connected) {
        //Serial.print("HttpClient>\tConnecting to: ");
        //Serial.print(aRequest.hostname);
        //Serial.print(":");
        //Serial.println(aRequest.port);
    } else {
        //Serial.println("HttpClient>\tConnection failed.");
    }
    #endif

    //
    // Send HTTP Headers
    //

    // Send initial headers (only HTTP 1.0 is supported for now).
    client.print(aHttpMethod);
    client.print(" ");
    client.print(aRequest.path);
    client.print(" HTTP/1.0\r\n");

    #ifdef LOGGING
    //Serial.println("HttpClient>\tStart of HTTP Request.");
    //Serial.print(aHttpMethod);
    //Serial.print(" ");
    //Serial.print(aRequest.path);
    //Serial.print(" HTTP/1.0\r\n");
    #endif

    // Send General and Request Headers.
    sendHeader("Connection", "close"); // Not supporting keep-alive for now.
    sendHeader("HOST", aRequest.hostname.c_str());

    // TODO: Support for connecting with IP address instead of URL
    // if (aRequest.hostname == NULL) {
    //     //sendHeader("HOST", ip);
    // } else {
    //     sendHeader("HOST", aRequest.hostname);
    // }


    //Send Entity Headers
    // TODO: Check the standard, currently sending Content-Length : 0 for empty
    // POST requests, and no content-length for other types.
    if (aRequest.body != NULL) {
        sendHeader("Content-Length", (aRequest.body).length());
    } else if (strcmp(aHttpMethod, HTTP_METHOD_POST) == 0) { //Check to see if its a Post method.
        sendHeader("Content-Length", 0);
    }

    if (headers != NULL)
    {
        int i = 0;
        while (headers[i].header != NULL)
        {
            if (headers[i].value != NULL) {
                sendHeader(headers[i].header, headers[i].value);
            } else {
                sendHeader(headers[i].header);
            }
            i++;
        }
    }

    // Empty line to finish headers
    client.println();
    client.flush();

    //
    // Send HTTP Request Body
    //

    if (aRequest.body != NULL) {
        client.println(aRequest.body);

        #ifdef LOGGING
        Serial.println(aRequest.body);
        #endif
    }

    #ifdef LOGGING
    //Serial.println("HttpClient>\tEnd of HTTP Request.");
    #endif

    //
    // Receive HTTP Response
    //
    // The first value of client.available() might not represent the
    // whole response, so after the first chunk of data is received instead
    // of terminating the connection there is a delay and another attempt
    // to read data.
    // The loop exits when the connection is closed, or if there is a
    // timeout or an error.

    unsigned int bufferPosition = 0;
    unsigned long lastRead = millis();
    unsigned long firstRead = millis();
    bool error = false;
    bool timeout = false;

    do {
        #ifdef LOGGING
        int bytes = client.available();
        if(bytes) {
            //Serial.print("\r\nHttpClient>\tReceiving TCP transaction of ");
            //Serial.print(bytes);
            //Serial.println(" bytes.");
        }
        #endif

        while (client.available()) {
            char c = client.read();
            #ifdef LOGGING
            Serial.print(c);
            #endif
            lastRead = millis();

            if (c == -1) {
                error = true;

                #ifdef LOGGING
                //Serial.println("HttpClient>\tError: No data available.");
                #endif

                break;
            }

            // Check that received character fits in buffer before storing.
            if (bufferPosition < sizeof(buffer)-1) {
                buffer[bufferPosition] = c;
            } else if ((bufferPosition == sizeof(buffer)-1)) {
                buffer[bufferPosition] = '\0'; // Null-terminate buffer
                client.stop();
                error = true;

                #ifdef LOGGING
                //Serial.println("HttpClient>\tError: Response body larger than buffer.");
                #endif
            }
            bufferPosition++;
        }

        #ifdef LOGGING
        if (bytes) {
            //Serial.print("\r\nHttpClient>\tEnd of TCP transaction.");
        }
        #endif

        // Check that there hasn't been more than 5s since last read.
        timeout = millis() - lastRead > TIMEOUT;

        // Unless there has been an error or timeout wait 200ms to allow server
        // to respond or close connection.
        if (!error && !timeout) {
            delay(200);
        }
    } while (client.connected() && !timeout && !error);

    #ifdef LOGGING
    if (timeout) {
       // Serial.println("\r\nHttpClient>\tError: Timeout while reading response.");
    }
    //Serial.print("\r\nHttpClient>\tEnd of HTTP Response (");
    //Serial.print(millis() - firstRead);
    //Serial.println("ms).");
    #endif
    client.stop();

    String raw_response(buffer);

    // Not super elegant way of finding the status code, but it works.
    String statusCode = raw_response.substring(9,12);

    #ifdef LOGGING
    //Serial.print("HttpClient>\tStatus Code: ");
    //Serial.println(statusCode);
    #endif

    int bodyPos = raw_response.indexOf("\r\n\r\n");
    if (bodyPos == -1) {
        #ifdef LOGGING
        //Serial.println("HttpClient>\tError: Can't find HTTP response body.");
        #endif

        return;
    }
    // Return the entire message body from bodyPos+4 till end.
    //aResponse.body = raw_response.substring((bodyPos+4));
    aResponse.body = raw_response;
    aResponse.status = atoi(statusCode.c_str());
}