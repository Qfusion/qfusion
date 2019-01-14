vec3 Greyscale(vec3 color)
{
	return vec3(dot(color, vec3(0.299, 0.587, 0.114)));
}
