var addon = require('./build/Release/mecab');

addon.mecabAsync("すももももももももものうち",function(er,result){
     console.log(result);
 });
