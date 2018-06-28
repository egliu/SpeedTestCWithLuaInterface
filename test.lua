require "SpeedTestC"
local inspect    = require("inspect");

-- SpeedTestC.InitSpeedTest();
-- local config = SpeedTestC.GetConfig();
-- if (config.errorCode ~= 0)
-- then
--     print(config.errorInfo);
-- else
--     print(inspect(config));
--     local bestServer = SpeedTestC.GetBestServer();
--     if (bestServer.errorCode ~= 0)
--     then
--         print(bestServer.errorInfo);
--     else
--         print(inspect(bestServer));
--         local speed = SpeedTestC.TestSpeed();
--         print(inspect(speed));
--     end
-- end

local function TS()
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
end

local function main()
    local count=0;
    local aveDel = 0;
    while(true)
    do
        print("current count is "..count);
        local t1 = os.time();
        TS();
        local delta = os.time() - t1;
        aveDel = (aveDel*count+delta)/(count+1);
        print("aveDel is "..aveDel.." ms");
        count = count + 1;
    end
end

main();
