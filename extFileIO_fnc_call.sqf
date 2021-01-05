params ["_method", "_args"];
private _result = "";
private _longResult = nil;
// do a call for an extra variable scope
0 call {
    ("extFileIO" callExtension [_method, _args]) params ["_resultData", "_returnCode", "_errorCode"];
    if (_errorCode != 0) then { throw _errorCode; };
    switch _returnCode do {
        case -1: { _result = (parseSimpleArray format["[%1]", _resultData]) select 0; throw _result; };
        case 0: { _result = (parseSimpleArray format["[%1]", _resultData]) select 0; };
        case 1: { _longResult = (parseSimpleArray format["[%1]", _resultData]) select 0; };
    };
};
// while in long result, keep polling
while { !isNil "_longResult" } do
{
    ("extFileIO" callExtension ["?", _longResult]) params ["_resultData", "_returnCode", "_errorCode"];
    if (_errorCode != 0) then { throw _errorCode; };
    switch _returnCode do {
        case -1: { _result = (parseSimpleArray format["[%1]", _result + _resultData]) select 0; throw _result; };
        case 0: { _result = (parseSimpleArray format["[%1]", _result + _resultData]) select 0; _longResult = nil; };
        case 1: { _result = _result + _resultData; };
    };
};
_result
