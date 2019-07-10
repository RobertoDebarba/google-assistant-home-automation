const AWS = require('aws-sdk');
const util = require('util')

var iotdata = new AWS.IotData({endpoint:"a183w3orhhc77-ats.iot.us-east-1.amazonaws.com"});

function publish(data, done) {
    var params = {
        topic: '$aws/things/arduino-thing/shadow/update',
        payload: '{"state":{"desired":' + data + '}}',
        qos: 0
    };
    
    iotdata.publish(params, function(err, data){
        if (err) {
            done(new Error(err));
        }
    });
}

function getThingShadow(callback, done) {
    var params = {
        thingName: 'arduino-thing'
    };
    
    iotdata.getThingShadow(params, function(err, data) {
        if (err) {
            done(new Error(err));
        } else {
            console.log("getThingShadow: " + util.inspect(data, {showHidden: false, depth: null}))
            callback(JSON.parse(data.payload));
        }
    });
}

function lightOn(done) {
    publish('{"led":"on"}', done);
}

function lightOff(done) {
    publish('{"led":"off"}', done);
}

function arConditioningOn(done) {
    publish('{"ar":"on"}', done);
}

function arConditioningOff(done) {
    publish('{"ar":"off"}', done);
}

function tvOn(done) {
    publish('{"tv":"on"}', done);
}


function tvOff(done) {
    publish('{"tv":"off"}', done);
}

function getTemperature(callback, done) {
    let data = getThingShadow(function(data) {
        let temp = Math.round(data.state.desired.temp);
        let hum = Math.round(data.state.desired.hum);
        
        callback("A temperatura atual dentro da sua casa é de " + temp + "° e a humidade está em " + hum + "%. Está um pouco quente. Deseja ligar o ar condicionado?");    
    }, done);
}

function processIntent(rqBody, done) {
    console.log("POST body: " + rqBody);
    
    let textToSpeech = "Ok";
    
    let body;
    try {
        body = JSON.parse(rqBody);
    } catch (e) {
        done();
        return;
    }
    
    if (body.queryResult == null) {
        done();
        return;
    }
    
    let intent = body.queryResult.intent.displayName;
    
    if (intent == 'light-on') {
        lightOn(done);
    }
    if (intent == 'light-off') {
        lightOff(done);
    }
    if (intent == 'tv-on') {
        tvOn(done);
    }
    if (intent == 'tv-off') {
        tvOff(done);
    }
    if (intent == 'ar-conditioning-on') {
        arConditioningOn(done);
    }
    if (intent == 'ar-conditioning-off') {
        arConditioningOff(done);
    }
    if (intent == 'ar-conditioning-ask-on') {
        let isTurnon = body.queryResult.parameters.turnon;
        
        if (isTurnon) {
            arConditioningOn(done);
        }
    }
    if (intent == 'get-temp') {
        getTemperature((message) => {
            done(null, {
              payload: {
                google: {
                  expectUserResponse: true,
                  richResponse: {
                    items: [
                      {
                        simpleResponse: {
                          textToSpeech: message
                        }
                      }
                    ]
                  }
                }
              },
              //followupEventInput: {
            //    name: "ar-conditioning-ask-on",
             //   languageCode: "pt-BR"
              //}
            });
        }, done);
        
        return;
    }
    
    done(null, {
      payload: {
        google: {
          expectUserResponse: true,
          richResponse: {
            items: [
              {
                simpleResponse: {
                  textToSpeech: textToSpeech
                }
              }
            ]
          }
        }
      }
    });
}

exports.handler = function(event, context, callback) {
    const done = (err, res) => {
        console.log("Result: " + JSON.stringify(res));
        
        callback(null, {
            statusCode: err ? '400' : '200',
            body: err ? err.message : JSON.stringify(res),
            headers: {
                'Content-Type': 'application/json; charset=utf-8',
            }
        });
    };

    switch (event.httpMethod) {
        case 'DELETE':
            done(new Error(`Unsupported method "${event.httpMethod}"`));
            break;
        case 'GET':
            done(new Error(`Unsupported method "${event.httpMethod}"`));
            break;
        case 'POST':
            console.log("New POST");
            processIntent(event.body, done);
            break;
        case 'PUT':
            done(new Error(`Unsupported method "${event.httpMethod}"`));
            break;
        case 'PATCH':
            publish(event.body, done);
            break;
        default:
            done(new Error(`Unsupported method "${event.httpMethod}"`));
    }
};