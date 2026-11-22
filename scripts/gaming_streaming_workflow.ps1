$ErrorActionPreference='Stop'
Set-Location 'E:\__PROJECTS__\OBS-MCP'
$id=0; $requests=New-Object System.Collections.ArrayList
function Add-Req([string]$method,$argObj){ $script:id++; $o=[ordered]@{jsonrpc='2.0';id=$script:id;method=$method}; if($argObj){$o.params=$argObj}; [void]$script:requests.Add(($o|ConvertTo-Json -Compress -Depth 20)) }
function Add-Tool([string]$name,$argObj){ Add-Req 'tools/call' ([ordered]@{name=$name;arguments=$argObj}) }
Add-Tool 'obs_launch' ([ordered]@{name='sender'})
Add-Tool 'obs_launch' ([ordered]@{name='receiver'})
Add-Tool 'wait_ms' ([ordered]@{ms=12000})
Add-Tool 'obs_cpu_stats' ([ordered]@{name='sender';window_ms=3000})
Add-Tool 'obs_cpu_stats' ([ordered]@{name='receiver';window_ms=3000})
Add-Tool 'obs_terminate' ([ordered]@{name='sender'})
Add-Tool 'obs_terminate' ([ordered]@{name='receiver'})
$requests | & 'build\obs-mcp.exe' --config 'config/gaming-streaming.toml' | Set-Content 'E:\__PROJECTS__\Wormhole\exp_results\obs-workflow-clean.jsonl' -Encoding UTF8
"responses=$(@(Get-Content 'E:\__PROJECTS__\Wormhole\exp_results\obs-workflow-clean.jsonl').Count) requests=$($requests.Count)"
