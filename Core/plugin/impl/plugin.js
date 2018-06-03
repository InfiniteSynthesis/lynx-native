(function(){function r(e,n,t){function o(i,f){if(!n[i]){if(!e[i]){var c="function"==typeof require&&require;if(!f&&c)return c(i,!0);if(u)return u(i,!0);var a=new Error("Cannot find module '"+i+"'");throw a.code="MODULE_NOT_FOUND",a}var p=n[i]={exports:{}};e[i][0].call(p.exports,function(r){var n=e[i][1][r];return o(n||r)},p,p.exports,r,e,n,t)}return n[i].exports}for(var u="function"==typeof require&&require,i=0;i<t.length;i++)o(t[i]);return o}return r})()({1:[function(require,module,exports){
var exec = require('../exec').exec

var Clipboard = {}
Clipboard.getString = function() {
    return new Promise((resolve, reject)=>{
                       exec('Clipboard', 'getString', [], function(content){ resolve(content) }, null)
                       });
}

Clipboard.setString = function(content) {
    exec('Clipboard', 'setString', [content], null, null)
}

module.exports = Clipboard


},{"../exec":2}],2:[function(require,module,exports){

function exec(module, method, args, onSuccess, onFail) {
    var callbackId = plugin.callbackId++
    if (onSuccess || onFail) {
        plugin.callbacks[callbackId] = {success:onSuccess, fail:onFail};
    }
    plugin.exec(module, method, callbackId, args)
}

function addEventListener(module, method, callback) {
    plugin.addEventListener(module, method, callback)
}

function removeEventListener(module, method, callback) {
    plugin.removeEventListener(module, method, callback)
}

function callbackFromNative(callbackId, successed, args) {
    
    var callback = plugin.callbacks[callbackId];
    if (callback) {
        if (successed) {
            callback.success.apply(null, args);
        } else {
            callback.fail.apply(null, args);
        }
        delete plugin.callbacks[callbackId];
    }
}

plugin.callbackId = 0
plugin.callbacks = {}
plugin.init(callbackFromNative)

var pluginExec = {}

pluginExec.addEventListener = addEventListener
pluginExec.removeEventListener = removeEventListener
pluginExec.exec = exec
module.exports = pluginExec

},{}],3:[function(require,module,exports){
(function (global){

global.NetInfo = require('./net_info/netinfo')
global.Clipboard = require('./clipboard/clipboard')

}).call(this,typeof global !== "undefined" ? global : typeof self !== "undefined" ? self : typeof window !== "undefined" ? window : {})
},{"./clipboard/clipboard":1,"./net_info/netinfo":4}],4:[function(require,module,exports){
var exec = require('../exec').exec
var addEventListener = require('../exec').addEventListener
var removeEventListener = require('../exec').removeEventListener
var NetInfo = {}

NetInfo.getConnectInfo = function() {
    return new Promise((resolve, reject)=>{
                       exec('NetInfo', 'getConnectInfo', [], function(info){ resolve(info) }, null)
                       });}

NetInfo.isConnected = function(callback) {
    exec('NetInfo', 'isConnected', [], callback, null)
}

NetInfo.addEventListener = function(event, callback) {
    addEventListener('NetInfo', event, callback)
}

NetInfo.removeEventListener = function(event, callback) {
    removeEventListener('NetInfo', event, callback)
}
module.exports = NetInfo

},{"../exec":2}]},{},[3]);
