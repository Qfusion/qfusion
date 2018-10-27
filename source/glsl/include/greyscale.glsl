myhalf3 Greyscale(myhalf3 color)
{
	return myhalf3(dot(color, myhalf3(0.299, 0.587, 0.114)));
}
