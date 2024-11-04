#
#
#

$RND = $(Get-Random -Minimum 10000 -Maximum 99999)

#$RND = 222222
$ResourceGroup = "ESP-RSGroupe$RND"
$location = "westeurope"
$StorageAccountName = "esptablestorage$RND"
$TableName = "EspLogData"

$azconnect = Get-AzContext  
if (!$azconnect) {  
    Connect-AzAccount  
} 

New-AzResourceGroup -ResourceGroupName $ResourceGroup -Location $location
New-AzStorageAccount -ResourceGroupName $ResourceGroup `
    -Name $StorageAccountName `
    -Location $location `
    -SkuName Standard_LRS `
    -Kind StorageV2 `
    -EnableHttpsTrafficOnly $false

# https://learn.microsoft.com/en-us/azure/storage/common/storage-network-security?tabs=azure-powershell
Update-AzStorageAccountNetworkRuleSet -ResourceGroupName $ResourceGroup -Name $StorageAccountName -DefaultAction Allow

#Set-AzStorageAccount -Name $StorageAccountName -ResourceGroupName $ResourceGroup -EnableHttpsTrafficOnly $false 
az storage account update -g $ResourceGroup -n $StorageAccountName --https-only false

#$Context = $storageAccount.Context
$Context = $(Get-AzStorageAccount -ResourceGroupName $ResourceGroup -Name $StorageAccountName).Context

# Creat the ( not so ) StaticWebsite 
Enable-AzStorageStaticWebsite -Context $Context -IndexDocument index.html -ErrorDocument404Path error.html
# get the url of the static website
$weburl = (Get-AzStorageAccount -ResourceGroupName $ResourceGroup -Name $StorageAccountName | Select-Object PrimaryEndpoints).PrimaryEndpoints.Web
$weburl = $weburl.TrimEnd('/')
write-host $weburl 

# Creat the Azure Table to store the data
New-AzStorageTable -Name $TableName -Context $Context

#########################################################################################################################################################
# Creat the Access-Control-Allow-Origin header so that the data cn be accessed by the StaticWebsite  
# https://learn.microsoft.com/en-us/rest/api/storageservices/cross-origin-resource-sharing--cors--support-for-the-azure-storage-services
#########################################################################################################################################################

$CorsRules = (
    @{
        AllowedOrigins  = @("$weburl"); 
        AllowedMethods  = @("Get", "Post");
        ExposedHeaders  = @("x-ms-continuation-NextRowKey", "x-ms-continuation-NextPartitionKey"); 
        AllowedHeaders  = @("Access-Control-Allow-Origin" );
        MaxAgeInSeconds = 0; 
    });
Remove-AzStorageCORSRule -ServiceType Table -Context $Context
Set-AzStorageCORSRule -ServiceType Table -Context $Context -CorsRules $CorsRules
Get-AzStorageCORSRule -ServiceType Table -Context $Context | ConvertTo-Json
$TableUrl = (Get-AzStorageAccount -ResourceGroupName $ResourceGroup -Name $StorageAccountName | Select-Object PrimaryEndpoints).PrimaryEndpoints.Table

#########################################################################################################################################################
# Creat 3 access keys one for read(web site) and one for append and read ( for the ESP ) and one for the adminstrator scrips   
#########################################################################################################################################################
$sasTokenRead = $(New-AzStorageAccountSASToken -Context $Context -Service Table -Permission "r" -Protocol HttpsOrHttp -ResourceType Object -ExpiryTime (Get-Date).AddDays(3650) )
$sasTokenRead = '?' + $sasTokenRead.TrimStart('?'); # The leading question mark '?' of the created SAS token will be removed in a future release.
$EndpointRead = "$TableUrl$TableName$sasTokenRead"
write-host $EndpointRead

$sasTokenAppend = $(New-AzStorageAccountSASToken -Context $Context -Service Table -Permission "rau" -Protocol HttpsOrHttp -ResourceType Object -ExpiryTime (Get-Date).AddDays(3650) )
$sasTokenAppend = '?' + $sasTokenAppend.TrimStart('?'); # The leading question mark '?' of the created SAS token will be removed in a future release.
$EndpointAppend = "$TableUrl$TableName$sasTokenAppend"
# now change the Endpoint to non secure HTTP connection as i do not use the HTTPS on the ESP32 as it is slow and then i need to maintaint the Root certificate. 
$httpEndpointAppend = $EndpointAppend.replace("https://", "http://")
write-host $EndpointAppend
write-host $httpEndpointAppend

$sasTokenAll = $(New-AzStorageAccountSASToken -Context $Context -Service Table -Permission "racwdlup" -Protocol HttpsOrHttp -ResourceType Object -ExpiryTime (Get-Date).AddDays(3650) )
$sasTokenAll = '?' + $sasTokenAll.TrimStart('?'); # The leading question mark '?' of the created SAS token will be removed in a future release.
$EndpointAll = "$TableUrl$TableName$sasTokenAll"
write-host $EndpointAll

#########################################################################################################################################################
#creat the Endpoint/SAS for Website and ESP32 ( HTTP & HTTPS)  
#########################################################################################################################################################
write-host "const TableEndpoint = '$EndpointRead';"
"const TableEndpoint = '$EndpointRead';" >.\web\endpoint.js
"#define AZURETABLEENDPOINT       `"$httpEndpointAppend`"" >.\src\TableEndpoint.hpp
"#define AZURETABLEHTTPSENDPOINT `"$EndpointAppend`"" >>.\src\TableEndpoint.hpp

#########################################################################################################################################################
# upload the Web files and set ContentType
#########################################################################################################################################################
#set-AzStorageblobcontent -File .\web\endpoint.js -Container "`$web" -Blob endpoint.js -Force -Context $Context -Properties @{"ContentType" = "text/javascript" }
#set-AzStorageblobcontent -File .\web\tablebuilder.js -Container "`$web" -Blob tablebuilder.js -Force -Context $Context -Properties @{"ContentType" = "text/javascript" }
#set-AzStorageblobcontent -File .\web\main.css -Container "`$web" -Blob main.css -Force -Context $Context -Properties @{"ContentType" = "text/css" }
#set-AzStorageblobcontent -File .\web\index.html -Container "`$web" -Blob index.html -Force -Context $Context -Properties @{"ContentType" = "text/html" }
#set-AzStorageblobcontent -File .\web\error.html -Container "`$web" -Blob error.html -Force -Context $Context -Properties @{"ContentType" = "text/html" }
#set-AzStorageblobcontent -File .\web\favicon.ico -Container "`$web" -Blob favicon.ico -Force -Context $Context -Properties @{"ContentType" = "image/x-icon" }

#########################################################################################################################################################
# upload the Web files recursively and set ContentType base on table $ContentType
#########################################################################################################################################################

$Context = $(Get-AzStorageAccount -ResourceGroupName $ResourceGroup -Name $StorageAccountName).Context

$ContentType = @{
    css  = "text/css"
    csv  = "text/csv" 
    htm  = "text/html"
    html = "text/html"
    ico  = "image/x-icon"    
    jpeg = "image/jpeg"
    js   = "application/javascript"   
    png  = "image/png"
    svg  = "image/svg+xml"    
    txt  = "text/plain"    
    xml  = "text/xml"
}

$WebRoot = ".\web";
$Root=Resolve-Path -Path $WebRoot
$Files = Get-ChildItem $WebRoot -Recurse -File
Foreach ($File in $Files) {
    set-AzStorageblobcontent -File $File.FullName -Container '$web' -Blob "$($File.FullName.Replace(`"$Root\`", `"`"))" -Force -Context $Context -Properties @{"ContentType" = "$($ContentType[$File.Extension.Trim('.')])" }
}


#########################################################################################################################################################
# Store one entry in the table 
# https://learn.microsoft.com/en-us/rest/api/storageservices/table-service-rest-api
# https://ryland.dev/posts/az-storage-tables-rest-powershell/#get-entity
# https://learn.microsoft.com/en-us/rest/api/storageservices/query-operators-supported-for-the-table-service
#########################################################################################################################################################
<#
.SYNOPSIS
This will Append a record to the Partition.
.DESCRIPTION
The Update-Month.ps1 script updates the registry with new data generated
during the past month and generates a report.
.PARAMETER EndPoint
Specifies the path Endpiont and SAS key
Somting line this 
https://XXXXXXXXXX.table.core.windows.net/TABLENAME?sv=2022-11-02&ss=t&srt=o&spr=https,http&se=2034-10-28T05%3A52%3A25Z&sp=ra&sig=QHHM9r11MTHJ4sKvMLELOS06h1legVrj6UDwuYWe250%3D  
.PARAMETER PartitionKey
This is the Partition that the data will be apppended
.PARAMETER $TableData
This point to a data object
#>
function AppendTableData {
    param (
        [Parameter(Mandatory = $true)][object] $EndPoint, 
        [Parameter(Mandatory = $true)][String] $PartitionKey, 
        [Parameter(Mandatory = $true)][object] $TableData 
         )
    $RowKey = (New-Guid).Guid
    $TableData += @{
        PartitionKey = $PartitionKey
        RowKey       = $RowKey
    }
    # Convert the entity to JSON
    $jsonEntity = $TableData | ConvertTo-Json -Compress
    $Header = @{ 'Accept' = 'application/json;odata=nometadata' }
    # Insert the entity into the table
    $response = Invoke-RestMethod -Method Post -Uri $EndPoint -Header $Header -Body $jsonEntity -ContentType "application/json"
    # Output the response
    return $response
}
###############################################################################################################################################################################


###############################################################################################################################################################################
<#
.SYNOPSIS
This will Update and Create a record to the Partition.
.DESCRIPTION
This will Update and Create a record to the Partition.
.PARAMETER EndPoint
Specifies the path Endpiont 
Somting line this 
https://XXXXXXXXXX.table.core.windows.net/TABLENAME?sv=2022-11-02&ss=t&srt=o&spr=https,http&se=2034-10-28T05%3A52%3A25Z&sp=ra&sig=QHHM9r11MTHJ4sKvMLELOS06h1legVrj6UDwuYWe250%3D  
.PARAMETER PartitionKey
This is the Partition that the data will be apppended
.PARAMETER $TableData
This point to a data object
#>
function UpdateTableData {
    param (
        [Parameter(Mandatory = $true)][object] $EndPoint, 
        [Parameter(Mandatory = $true)][String] $PartitionKey, 
        [Parameter(Mandatory = $true)][object] $TableData, 
        [Parameter(Mandatory = $true)][String] $Key )
    $Value = $TableData[$Key]
    $jsonEntity = $TableData | ConvertTo-Json -Compress
    $Header = @{ 'Accept' = 'application/json;odata=nometadata' }
    $filter = "&`$filter=PartitionKey eq '$PartitionKey' and $Key eq '$Value'&`$top=1"
    $readresp = Invoke-RestMethod -Method Get -Uri "$EndPoint$filter" -Header $Header -ContentType "application/json"  
    if ( $readresp.value.Count -gt 0 )
    { 
        # update
        $filter = "(PartitionKey='$($readresp.value[0].PartitionKey)',RowKey='$($readresp.value[0].RowKey)')";
        $EndPoint = $EndPoint.Replace('?', "$filter`?")
        $Header = @{
            'Accept'   = 'application/json;odata=nometadata'
            'If-Match' = '*' 
        }
        $response = Invoke-RestMethod -Method Put -Uri "$EndPoint" -Header $Header -Body $jsonEntity -ContentType "application/json"
    } else {
        $response=AppendTableData $EndPoint $PartitionKey $TableData 
    }
    rerurn $response
}
###############################################################################################################################################################################

$ParameterTable = @{
    DeviceName     = ""    
    LedColor       = (3 * 65536) + (3 * 256) + 3
    LedInterval    = 1001
    ReloadInterval = 60 * 60
    TemperatureInterval =  60 * 5
}
UpdateTableData $EndpointAppend "Parameters" $ParameterTable "DeviceName" 

$ParameterTable.DeviceName  = "ESP-TABLE_ESP32-S3_68:B6:B3:20:F8:1C"
$ParameterTable.LedInterval = 5001
$ParameterTable.LedColor    = (2 * 65536) + (1 * 256) + 3
$ParameterTable.TemperatureInterval = 50
UpdateTableData $EndpointAppend "Parameters" $ParameterTable "DeviceName" 

Invoke-RestMethod -Method Get -Uri "$httpEndpointAppend&`$filter=PartitionKey eq 'Parameters'" -Header $Header  -ContentType "application/json" | ConvertTo-Json

$TableData = @{    
    MyFreeText = "Yes this is a powershell append "
    Property2 = "value2"
    Property3 = "value3"
}

AppendTableData $EndpointAppend "PowershellScript" $TableData 

#########################################################################################################################################################
# test a none secure Http append 
#
#########################################################################################################################################################

$TableData = @{    
    MyFreeText = "Yes this is a powershell noncecure HTTP append "
    Property2  = "value2"
    Property3  = "value3"
}
AppendTableData $httpEndpointAppend "PowershellScript" $TableData 

Invoke-RestMethod -Method Get -Uri "$httpEndpointAppend&`$filter=PartitionKey eq 'PowershellScript'" -Header $Header  -ContentType "application/json" | ConvertTo-Json

###############################################################################################################################################################################
<#
.SYNOPSIS
This will get the entrys of the the Partition in blocks of 1000 entrys
.DESCRIPTION
This will get the entrys of the the Partition in blocks of 1000 entrys
.PARAMETER EndPoint
Specifies the path Endpiont 
Somting line this 
https://XXXXXXXXXX.table.core.windows.net/TABLENAME?sv=2022-11-02&ss=t&srt=o&spr=https,http&se=2034-10-28T05%3A52%3A25Z&sp=ra&sig=QHHM9r11MTHJ4sKvMLELOS06h1legVrj6UDwuYWe250%3D  
.PARAMETER Filter
This is the Filter that will be used 
.PARAMETER Select
This will select the coloms of the data , default is all coloms
#>
function GetLageTableData {
param (
    [Parameter(Mandatory = $true)][object] $EndPoint, 
    [Parameter(Mandatory = $true)][String] $Filter,
    [Parameter(Mandatory = $false)][String] $select 
    )

if ([string]::IsNullOrWhiteSpace($select)) { $select=""; } # This will select all
# https://learn.microsoft.com/en-us/rest/api/storageservices/Query-Timeout-and-Pagination?redirectedfrom=MSDN
$selection = @{
    'StatusCode' = 0
    'value' =   @()
}

$Header = @{
    'Accept' = 'application/json;odata=nometadata' 
}
do 
{
    $response = Invoke-WebRequest  -Method Get -Uri "$httpEndpointAppend&$Filter$select" -Header $Header  -ContentType "application/json"
    $selection.StatusCode = $response.StatusCode
    if ($response.StatusCode -eq 200) {
        $json=ConvertFrom-Json $([String]::new($response.Content))
        $selection.value+= $json.value 
    }    
    $NextPartitionKey = $response.Headers.'x-ms-continuation-NextPartitionKey'
    $NextRowKey = $response.Headers.'x-ms-continuation-NextRowKey'
    $Filter = "NextPartitionKey=$NextPartitionKey&NextRowKey=$NextRowKey$select"
    } while ($NextPartitionKey -And ($response.StatusCode -eq 200) )
    Return $selection
}
###############################################################################################################################################################################

# This will insert 1110 entrys in the table, and then you can only get 1000 back.
<#
$TableData = @{    
    MyFreeText = "Yes this is a powershell append "
}
1..2110 | % {
    $TableData.MyFreeText = "Created a Entry $_" 
    $e=
}
#>

$Filter = "`$filter=PartitionKey eq 'PowershellScript'"
$select = "&`$select=Timestamp,MyFreeText"
$val = GetLageTableData $httpEndpointAppend $Filter $select 
$val.value.Length
$val.value | ConvertTo-Json

GetLageTableData $httpEndpointAppend "`$filter=PartitionKey eq 'Parameters'" | ConvertTo-Json -Depth 4

#########################################################################################################################################
# delete old table data
# PartitionKey eq 'ESPReboots' and Timestamp le datetime'2024-10-28T09:00:58.000Z'
# $EndpointAll ="https://esptablestorage127203.table.core.windows.net/EspLogData?sv=2022-11-02&ss=t&srt=o&spr=https,http&se=2034-10-26T19%3A15%3A44Z&sp=rwdlacup&sig=0iKxAnwJpM4twqCNpwm5IU5iWiTFCMwi2klY0uKKCZM%3D"
# $RND = 127203
# $StorageAccountName = "esptablestorage$RND"
# $TableName = "EspLogData"
# $TableUrl = "https://$StorageAccountName.table.core.windows.net/"
# $TableUrl = "https://esptablestorage127203.table.core.windows.net/"
# $TableUrl = (Get-AzStorageAccount -ResourceGroupName $ResourceGroup -Name $StorageAccountName | select PrimaryEndpoints).PrimaryEndpoints.Table

$Deletedate = $([DateTime]::UtcNow.AddDays(-1).ToString('u').replace(' ','T'))
$Deletedate = $([DateTime]::UtcNow.AddHours(-1).ToString('u').replace(' ', 'T'))

write-host "Delete old Reboots records older then $Deletedate"
$PartitionKey ="ESPReboots"
$Header = @{
    'Accept' = 'application/json;odata=nometadata' 
}
$response = Invoke-RestMethod -Method Get -Uri $EndpointAll"`&`$filter=PartitionKey eq `'$PartitionKey`' and Timestamp le datetime`'$Deletedate`'" -Header $Header -ContentType "application/json"
# delete all the separate reacords as the filter will not work for the Delete rest API 
foreach ($Row IN $response.value)
{
    $RowKey = $Row.RowKey;
    $PartitionKey = $Row.PartitionKey;
    $selector = "(PartitionKey='$PartitionKey',RowKey='$RowKey')";
    $EndPoint = $EndpointAll.Replace('?', "$selector`?")
    $Header = @{
        'Accept'   = 'application/json;odata=nometadata'
        'If-Match' = '*' 
    }
    Invoke-RestMethod -Method Delete -Uri "$EndPoint" -Header $Header -ContentType "application/json"
}
write-host "Deleted $($response.value.Length) entrys"

################################################################################################################################################
# Optimizing Azure Table Storage: Automated Data Cleanup using a PowerShell script with Azure Automate
# https://techcommunity.microsoft.com/t5/azure-paas-blog/optimizing-azure-table-storage-automated-data-cleanup-using-a/ba-p/4233955
#
#$automationAccount = "AutomationAccount"
#New-AzAutomationAccount -Name $automationAccount -Location $location -ResourceGroupName $ResourceGroup -AssignSystemIdentity

#$SAMI = (Get-AzAutomationAccount -ResourceGroupName $ResourceGroup -Name $automationAccount).Identity.PrincipalId
#New-AzRoleAssignment -ObjectId $SAMI -ResourceGroupName $resourceGroup -RoleDefinitionName $role1

################################################################################################################################################
# Get Some selects outputs

$Header = @{
    'Accept' = 'application/json;odata=nometadata' 
}
Invoke-RestMethod -Method Get -Uri $httpEndpointAppend -Header $Header  -ContentType "application/json" | ConvertTo-Json 
Invoke-RestMethod -Method Get -Uri "$httpEndpointAppend&`$filter=PartitionKey eq 'Parameters'" -Header $Header  -ContentType "application/json" | ConvertTo-Json 
Invoke-RestMethod -Method Get -Uri "$httpEndpointAppend&`$filter=PartitionKey eq 'Parameters'&`$top=1 " -Header $Header  -ContentType "application/json" | ConvertTo-Json 
Invoke-RestMethod -Method Get -Uri "$httpEndpointAppend&`$filter=PartitionKey eq 'Parameters'&`$select=DeviceName,LedColor " -Header $Header  -ContentType "application/json" | ConvertTo-Json 
Invoke-RestMethod -Method Get -Uri "$httpEndpointAppend&`$filter=PartitionKey eq 'Parameters' and DeviceName eq 'ESP-TABLE_ESP32-S3_68:B6:B3:20:F8:1C'&`$top=1 " -Header $Header  -ContentType "application/json" | ConvertTo-Json

Invoke-WebRequest -Method Get -Uri "$httpEndpointAppend&`$filter=PartitionKey eq 'Parameters' and DeviceName eq 'ESP-TABLE_ESP32-S3_68:B6:B3:20:F8:1C'&`$top=1 " -Header $Header  -ContentType "application/json" | ConvertTo-Json 
Invoke-RestMethod -Method Get -Uri "$httpEndpointAppend&`$filter=PartitionKey eq 'PowershellScript'" -Header $Header  -ContentType "application/json" | ConvertTo-Json 
Invoke-RestMethod -Method Get -Uri "$httpEndpointAppend&`$filter=PartitionKey eq 'ESPReboot'" -Header $Header  -ContentType "application/json" | ConvertTo-Json 
Invoke-RestMethod -Method Get -Uri "$httpEndpointAppend&`$filter=PartitionKey eq 'CoreTemperatures'" -Header $Header  -ContentType "application/json" | ConvertTo-Json 

# get the Restart for the last hour
$Deletedate = $([DateTime]::UtcNow.AddHours(-1).ToString('u').replace(' ', 'T'))
$PartitionKey="ESPReboots"
Invoke-RestMethod -Method Get -Uri $httpEndpointAppend"`&`$filter=PartitionKey eq `'$PartitionKey`' and Timestamp ge datetime`'$Deletedate`'" -Header $Header -ContentType "application/json" | ConvertTo-Json 

write-host "`$EndpointRead = '$EndpointRead'"
write-host "`$EndpointAppend = '$EndpointAppend'"
write-host "`$EndpointAll = '$EndpointAll'"
write-host "`$weburl = '$weburl'" 

Write-Host -NoNewLine 'Press any key to continue...';
$null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown');

########################################################################################################################################################