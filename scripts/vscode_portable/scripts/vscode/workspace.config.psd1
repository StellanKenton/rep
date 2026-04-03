@{
    ProjectPath = 'Project/MDK-ARM/mcos-gd32.uvprojx'
    KeilTarget = 'MCOS-GD32'
    HexFilePath = 'Project/MDK-ARM/GD32F4xx-OBJ/mcos-gd32.hex'

    JLink = @{
        Device = 'GD32F407VG'
        Interface = 'SWD'
        SpeedKHz = 10000
        GdbPort = 3331
        SwoPort = 3332
        TelnetPort = 3333
        RttTelnetPort = 19021
    }

    Serial = @{
        BaudRate = 115200
        DataBits = 8
        Parity = 'None'
        StopBits = 'One'
        DtrEnable = $false
        RtsEnable = $false
    }
}