using UnrealBuildTool;
using System.Collections.Generic;

public class DismambermentEditorTarget : TargetRules
{
    public DismambermentEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
        CppStandard = CppStandardVersion.Cpp20;
        //BuildEnvironment = TargetBuildEnvironment.Unique;
        ExtraModuleNames.Add("Dismamberment");
    }
}