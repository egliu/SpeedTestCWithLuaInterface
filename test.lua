require "SpeedTestC"
local inspect    = require("inspect");

SpeedTestC.InitSpeedTest();
local config = SpeedTestC.GetConfig();
if (config.errorCode ~= 0)
then
    print(config.errorInfo);
else
    print(inspect(config));
    local bestServer = SpeedTestC.GetBestServer();
    if (bestServer.errorCode ~= 0)
    then
        print(bestServer.errorInfo);
    else
        print(inspect(bestServer));
        local speed = SpeedTestC.TestSpeed();
        print(inspect(speed));
    end
end
