#version 400
// 杜绝声明未使用的变量，避免bug的产生。


in vec3 EntryPoint;
in vec4 ExitPointCoord;

uniform sampler2D ExitPoints;
uniform sampler3D VolumeTex;
uniform sampler1D TransferFunc;  
uniform float     StepSize;
uniform vec2      ScreenSize;
layout (location = 0) out vec4 FragColor;

void main()
{
    // ExitPointCoord 的坐标是设备规范化坐标
    // 出现了和纹理坐标有关的问题。
    vec3 exitPoint = texture(ExitPoints, gl_FragCoord.st/ScreenSize).xyz;
    // that will actually give you clip-space coordinates rather than
    // normalised device coordinates, since you're not performing the perspective
    // division which happens during the rasterisation process (between the vertex
    // shader and fragment shader
    //vec2 exitFragCoord = (ExitPointCoord.xy / ExitPointCoord.w + 1.0)/2.0;
    //vec3 exitPoint  = texture(ExitPoints, exitFragCoord).xyz;


    if (EntryPoint == exitPoint)
        //background need no raycasting
        discard;
    vec3 dir = exitPoint - EntryPoint;
    float len = length(dir); // the length from front to back is calculated and used to terminate the ray
    vec3 deltaDir = normalize(dir) * StepSize;
    vec4 colorAcum = vec4(0.0); // The dest color
    // backgroundColor
    vec4 bgColor = vec4(0.0, 0.0, 0.0, 1.0);

	int steps = int(len / StepSize);
  
    float max_intensity = 0;
    for(int i = 0; i < steps; i++)
    {
        // 获得体数据中的标量值scaler value
		vec3 voxelCoord = EntryPoint + deltaDir * i;
        float intensity =  texture(VolumeTex, voxelCoord * 1.0f).x;
        
        if (intensity > max_intensity){
            max_intensity = intensity;
        }

//         // 查找传输函数中映射后的值
//         // 依赖性纹理读取  
//         vec4 colorSample = texture(TransferFunc, intensity);
// 		colorAcum = colorAcum + colorSample * vec4(colorSample.aaa, 1.0) * (1.0 - colorAcum.a);
       
// 		if (colorAcum.a > 0.99){
//             break;
//         }
    }
    vec4 colorSample = texture(TransferFunc, max_intensity);
    // vec4 colorSample = vec4(max_intensity);
    colorAcum = colorSample;
    colorAcum = colorAcum + bgColor * (1.0 - colorAcum.a);
    colorAcum.a = 1.0;
    FragColor = colorAcum;
    // for test
    //FragColor = vec4(1.0);
}
