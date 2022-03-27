#version 400
// 杜绝声明未使用的变量，避免bug的产生。


in vec3 EntryPoint;
in vec4 ExitPointCoord;

uniform sampler2D ExitPoints;
uniform sampler3D VolumeTexX;
uniform sampler3D VolumeTexY;
uniform sampler3D VolumeTexZ;
uniform sampler1D TransferFunc;  
uniform float     StepSize;
uniform vec2      ScreenSize;
uniform vec3      weight;
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
  
    for(int i = 0; i < steps; i++)
    {
        // 获得体数据中的标量值scaler value
		vec3 voxelCoord = EntryPoint + deltaDir * i;
        float intensity_x =  texture(VolumeTexX, voxelCoord * 1.0f).x;
        float intensity_y =  texture(VolumeTexY, voxelCoord * 1.0f).x;
        float intensity_z =  texture(VolumeTexZ, voxelCoord * 1.0f).x;
        float intensity = intensity_x * weight[0] + intensity_y * weight[1] + intensity_z * weight[2];

        // 查找传输函数中映射后的值
        // 依赖性纹理读取  
        vec4 colorSample = texture(TransferFunc, intensity);
		colorAcum = colorAcum + colorSample * vec4(colorSample.aaa, 1.0) * (1.0 - colorAcum.a);
       
		if (colorAcum.a > 0.99){
            break;
        }
    }
	colorAcum = colorAcum + bgColor * (1.0 - colorAcum.a);
    colorAcum.a = 1.0;
    FragColor = colorAcum;
    // for test
    //FragColor = vec4(exitPoint, 1.0);
}
